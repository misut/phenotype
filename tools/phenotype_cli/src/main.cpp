import cppx.cli;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import file_explorer_shared;
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
    fs::path source;
    fs::path destination;
    std::uintmax_t bytes = 0;
    bool copied = false;
    std::string error;
};

struct BundleSummary {
    fs::path package_root;
    fs::path output_root;
    fs::path manifest_path;
    PackageSummary package;
    std::vector<BundleFileRecord> files;
    std::vector<Check> checks;
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
                        },
                        .examples = {
                            "phenotype artifact verify-file-explorer",
                            "phenotype artifact verify-file-explorer --json",
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
                            {.name = "input",
                             .arity = cppx::cli::OptionArity::one,
                             .repeatable = true,
                             .value_name = "kind[:value]",
                             .description = "Typed input such as viewport:900x620@2, select:README.txt, create-file, delete, sort:kind"},
                        },
                        .allow_positionals = false,
                        .examples = {
                            "phenotype drive file-explorer --json --input select:README.txt --input duplicate",
                            "phenotype drive file-explorer --profile mobile --scenario created-preview --json",
                        },
                    },
                },
                .allow_positionals = false,
                .category = "runtime",
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

auto quoted_array_for_key(std::string_view line, std::string_view key)
    -> std::optional<std::vector<std::string>> {
    auto trimmed = strip_toml_comment(line);
    if (!trimmed.starts_with(key))
        return std::nullopt;
    auto rest = trim_copy(std::string_view{trimmed}.substr(key.size()));
    if (!rest.starts_with("="))
        return std::nullopt;
    rest = trim_copy(std::string_view{rest}.substr(1));
    if (rest.size() < 2 || rest.front() != '[' || rest.back() != ']')
        return std::nullopt;

    auto values = std::vector<std::string>{};
    auto token = std::string{};
    auto in_string = false;
    auto escaped = false;
    for (std::size_t i = 1; i + 1 < rest.size(); ++i) {
        auto ch = rest[i];
        if (!in_string) {
            if (ch == '"') {
                in_string = true;
                token.clear();
            }
            continue;
        }
        if (escaped) {
            switch (ch) {
            case 'n': token.push_back('\n'); break;
            case 'r': token.push_back('\r'); break;
            case 't': token.push_back('\t'); break;
            default: token.push_back(ch); break;
            }
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            escaped = true;
            continue;
        }
        if (ch == '"') {
            values.push_back(token);
            in_string = false;
            continue;
        }
        token.push_back(ch);
    }
    return values;
}

auto bool_value_for_key(std::string_view line, std::string_view key)
    -> std::optional<bool> {
    auto trimmed = strip_toml_comment(line);
    if (!trimmed.starts_with(key))
        return std::nullopt;
    auto rest = trim_copy(std::string_view{trimmed}.substr(key.size()));
    if (!rest.starts_with("="))
        return std::nullopt;
    rest = trim_copy(std::string_view{rest}.substr(1));
    if (rest == "true")
        return true;
    if (rest == "false")
        return false;
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
    auto strings = std::vector<phenotype::LocaleString>{};
    auto text = read_text_file(path);
    auto input = std::istringstream{text};
    auto line = std::string{};
    auto section = std::string{};
    while (std::getline(input, line)) {
        auto trimmed = strip_toml_comment(line);
        if (trimmed.empty())
            continue;
        if (trimmed.size() >= 2 && trimmed.front() == '['
            && trimmed.back() == ']') {
            section = trim_copy(std::string_view{trimmed}.substr(
                1,
                trimmed.size() - 2));
            continue;
        }
        auto eq = trimmed.find('=');
        if (eq == std::string::npos)
            continue;
        auto key = trim_copy(std::string_view{trimmed}.substr(0, eq));
        if (key.empty())
            continue;
        auto value = quoted_value_for_key(trimmed, key);
        if (!value)
            continue;
        auto full_key = section.empty() ? key : section + "." + key;
        strings.push_back({
            .key = std::move(full_key),
            .value = std::move(*value),
        });
    }
    return strings;
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
        auto input = std::istringstream{text};
        auto line = std::string{};
        enum class Section {
            None,
            Application,
            Resources,
            Debug,
            Asset,
            Locale,
            Font,
        };
        auto section = Section::None;
        while (std::getline(input, line)) {
            auto trimmed = strip_toml_comment(line);
            if (trimmed.empty())
                continue;

            if (trimmed == "[application]") {
                summary.application_section = true;
                section = Section::Application;
                continue;
            }
            if (trimmed == "[resources]") {
                summary.resources_section = true;
                section = Section::Resources;
                continue;
            }
            if (trimmed == "[debug]") {
                summary.debug_section = true;
                section = Section::Debug;
                continue;
            }
            if (trimmed == "[[assets]]") {
                summary.catalog.assets.push_back({});
                section = Section::Asset;
                continue;
            }
            if (trimmed == "[[locales]]") {
                summary.catalog.locales.push_back({});
                section = Section::Locale;
                continue;
            }
            if (trimmed == "[[fonts]]") {
                summary.catalog.fonts.push_back({});
                section = Section::Font;
                continue;
            }

            if (section == Section::Application) {
                if (auto value = quoted_value_for_key(trimmed, "id")) {
                    summary.catalog.application.id = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "display_name")) {
                    summary.catalog.application.display_name = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "version")) {
                    summary.catalog.application.version = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "entry")) {
                    summary.catalog.application.entry = *value;
                    continue;
                }
                if (auto value = quoted_array_for_key(trimmed, "platforms")) {
                    summary.catalog.application.platforms = std::move(*value);
                    continue;
                }
            }
            if (section == Section::Resources) {
                if (auto value = quoted_value_for_key(trimmed, "default_locale")) {
                    summary.catalog.default_locale = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "default_font_family")) {
                    summary.catalog.default_font_family = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "artifact_manifest")) {
                    summary.artifact_manifest = *value;
                    summary.artifact_manifest_exists =
                        path_exists(summary.root / *value);
                    continue;
                }
            }
            if (section == Section::Debug) {
                if (auto value = quoted_value_for_key(trimmed, "probe_scene")) {
                    summary.catalog.debug.probe_scene = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "verifier")) {
                    summary.catalog.debug.verifier = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "artifact_manifest")) {
                    summary.catalog.debug.artifact_manifest = *value;
                    if (summary.artifact_manifest.empty()) {
                        summary.artifact_manifest = *value;
                        summary.artifact_manifest_exists =
                            path_exists(summary.root / *value);
                    }
                    continue;
                }
            }
            if (section == Section::Asset && !summary.catalog.assets.empty()) {
                auto& asset = summary.catalog.assets.back();
                if (auto value = quoted_value_for_key(trimmed, "name")) {
                    asset.name = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "source")) {
                    asset.source = *value;
                    ++summary.source_reference_count;
                    if (!path_exists(summary.root / *value))
                        summary.missing_sources.push_back(*value);
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "content_type")) {
                    asset.content_type = *value;
                    continue;
                }
                if (auto value = bool_value_for_key(trimmed, "preload")) {
                    asset.preload = *value;
                    continue;
                }
                if (auto value = bool_value_for_key(trimmed, "runtime_visible")) {
                    asset.runtime_visible = *value;
                    continue;
                }
            }
            if (section == Section::Locale && !summary.catalog.locales.empty()) {
                auto& locale = summary.catalog.locales.back();
                if (auto value = quoted_value_for_key(trimmed, "tag")) {
                    locale.tag = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "source")) {
                    locale.source = *value;
                    ++summary.source_reference_count;
                    if (!path_exists(summary.root / *value))
                        summary.missing_sources.push_back(*value);
                    continue;
                }
                if (auto value = quoted_array_for_key(trimmed, "fallback")) {
                    locale.fallback = std::move(*value);
                    continue;
                }
            }
            if (section == Section::Font && !summary.catalog.fonts.empty()) {
                auto& font = summary.catalog.fonts.back();
                if (auto value = quoted_value_for_key(trimmed, "family")) {
                    font.family = *value;
                    continue;
                }
                if (auto value = quoted_value_for_key(trimmed, "source")) {
                    font.source = *value;
                    ++summary.source_reference_count;
                    if (!path_exists(summary.root / *value))
                        summary.missing_sources.push_back(*value);
                    continue;
                }
                if (auto value = bool_value_for_key(trimmed, "register")) {
                    font.register_font = *value;
                    continue;
                }
                if (auto value = quoted_array_for_key(trimmed, "fallback")) {
                    font.fallback = std::move(*value);
                    continue;
                }
            }
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
        "{{\"kind\":{},\"name\":{},\"source\":{},\"destination\":{},"
        "\"bytes\":{},\"copied\":{},\"error\":{}}}",
        json_string(file.kind),
        json_string(file.name),
        json_string(path_string(file.source)),
        json_string(ec ? path_string(file.destination)
                       : path_string(relative_destination)),
        file.bytes,
        file.copied ? "true" : "false",
        json_string(file.error));
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
        "{{\"schema_version\":1,\"command\":\"package bundle\","
        "\"ok\":{},\"package_root\":{},\"output_root\":{},"
        "\"bundle_manifest\":{},\"application\":{{\"id\":{},"
        "\"display_name\":{},\"version\":{},\"entry\":{},"
        "\"platforms\":{}}},\"defaults\":{{\"locale\":{},"
        "\"font_family\":{}}},\"debug\":{{\"artifact_manifest\":{},"
        "\"probe_scene\":{},\"verifier\":{}}},\"file_count\":{},"
        "\"files\":{},\"checks\":{},\"error\":{}}}",
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
                      fs::path relative_source) -> BundleFileRecord {
    BundleFileRecord record{
        .kind = std::move(kind),
        .name = std::move(name),
        .source = package_root / relative_source,
        .destination = output_root / relative_source,
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
    return record;
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

    bundle.files.push_back(copy_bundle_file(
        bundle.package_root,
        bundle.output_root,
        "manifest",
        "phenotype.package.toml",
        "phenotype.package.toml"));

    for (auto const& asset : bundle.package.catalog.assets) {
        bundle.files.push_back(copy_bundle_file(
            bundle.package_root,
            bundle.output_root,
            "asset",
            asset.name,
            asset.source));
    }
    for (auto const& locale : bundle.package.catalog.locales) {
        bundle.files.push_back(copy_bundle_file(
            bundle.package_root,
            bundle.output_root,
            "locale",
            locale.tag,
            locale.source));
    }
    for (auto const& font : bundle.package.catalog.fonts) {
        bundle.files.push_back(copy_bundle_file(
            bundle.package_root,
            bundle.output_root,
            "font",
            font.family,
            font.source));
    }
    if (!bundle.package.catalog.debug.artifact_manifest.empty()) {
        bundle.files.push_back(copy_bundle_file(
            bundle.package_root,
            bundle.output_root,
            "debug",
            "artifact_manifest",
            bundle.package.catalog.debug.artifact_manifest));
    }

    auto copied = std::ranges::all_of(bundle.files, [](auto const& file) {
        return file.copied;
    });
    if (!copied) {
        bundle.error = "one or more package resources failed to copy";
        return bundle;
    }

    bundle.manifest_path = bundle.output_root / "phenotype.bundle.json";
    bundle.ok = true;
    auto manifest = bundle_json(bundle);
    if (!write_text_file(bundle.manifest_path, manifest, bundle.error)) {
        bundle.ok = false;
        return bundle;
    }
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

auto explorer_chrome_json(
        file_explorer_demo::ExplorerChromeMetrics const& chrome) -> std::string {
    return std::format(
        "{{\"viewport\":{{\"w\":{},\"h\":{},\"scale\":{}}},"
        "\"integrated_titlebar_height\":{},\"sidebar_width\":{},"
        "\"sidebar_row_width\":{},\"toolbar_group_height\":{},"
        "\"toolbar_group_radius\":{},\"window_radius\":{},"
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
        chrome.toolbar_group_height,
        chrome.toolbar_group_radius,
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

auto explorer_drive_json(file_explorer_demo::ExplorerDriveResult const& result)
    -> std::string {
    auto const& snap = result.snapshot;
    return std::format(
        "{{\"schema_version\":1,\"command\":\"drive file-explorer\","
        "\"ok\":{},\"profile\":{},\"input_count\":{},"
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
        "\"operation\":{},\"entries\":{},\"trace\":{}}}",
        explorer_drive_ok(result) ? "true" : "false",
        json_string(result.profile),
        result.trace.size(),
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
        explorer_trace_array_json(result.trace));
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

    auto args = std::vector<std::string>{
        "exec",
        "--",
        "uv",
        "run",
        "--frozen",
        "python",
        "tools/verify_artifact_bundle.py",
        absolute_path_string(*path),
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

    auto spec = cppx::process::ProcessSpec{
        .program = "mise",
        .args = std::move(args),
        .cwd = *root,
        .timeout = std::chrono::minutes{5},
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

int run_artifact_verify_glass_showcase(
        cppx::cli::Invocation const& invocation) {
    auto env = std::map<std::string, std::string>{};
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
             .status = std::ranges::all_of(bundle.files, [](auto const& file) {
                 return file.copied;
             }) ? cppx::terminal::StatusKind::ok
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

    auto result = file_explorer_demo::drive_explorer(normalized_profile, inputs);
    if (invocation.has("json")) {
        std::println("{}", explorer_drive_json(result));
    } else {
        auto const& snap = result.snapshot;
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "profile",
             .value = result.profile,
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
    return explorer_drive_ok(result) ? 0 : 1;
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
        == std::vector<std::string>{"phenotype", "drive", "file-explorer"})
        return run_drive_file_explorer(*parsed);
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

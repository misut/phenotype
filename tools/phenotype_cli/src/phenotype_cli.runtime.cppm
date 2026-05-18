export module phenotype_cli.runtime;

import cppx.process;
import cppx.process.system;
import cppx.terminal;
import phenotype_cli.common;
import std;

export namespace phenotype_cli::runtime {

namespace fs = std::filesystem;
using namespace phenotype_cli::common;

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

auto strip_toml_comment(std::string_view line) -> std::string {
    auto out = std::string{};
    out.reserve(line.size());
    auto in_string = false;
    auto escaped = false;
    for (char ch : line) {
        if (escaped) {
            out.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\' && in_string) {
            out.push_back(ch);
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_string = !in_string;
            out.push_back(ch);
            continue;
        }
        if (ch == '#' && !in_string)
            break;
        out.push_back(ch);
    }
    auto first = out.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return {};
    auto last = out.find_last_not_of(" \t\r\n");
    return out.substr(first, last - first + 1);
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
        char ch = rest[i];
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

auto output_line_count(std::string_view text) -> std::size_t {
    if (text.empty())
        return 0;
    return static_cast<std::size_t>(std::ranges::count(text, '\n'))
        + (text.back() == '\n' ? 0 : 1);
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

auto environment_value(std::string_view name) -> std::optional<std::string> {
    auto key = std::string{name};
    auto const* value = std::getenv(key.c_str());
    if (value == nullptr || value[0] == '\0')
        return std::nullopt;
    return std::string{value};
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

auto artifact_detail_json(ArtifactSummary const& artifact) -> std::string {
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

} // namespace phenotype_cli::runtime

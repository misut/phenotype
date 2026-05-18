export module phenotype_cli.common;

import cppx.cli;
import cppx.terminal;
import json;
import phenotype.resources;
import std;

export namespace phenotype_cli::common {

struct Check {
    std::string name;
    bool ok = false;
    std::string detail;
    std::string hint;
};

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

auto path_string(std::filesystem::path const& path) -> std::string {
    return path.lexically_normal().generic_string();
}

auto absolute_path_string(std::filesystem::path const& path) -> std::string {
    auto ec = std::error_code{};
    auto absolute = std::filesystem::absolute(path, ec);
    return ec ? path_string(path) : path_string(absolute);
}

auto read_text_file(std::filesystem::path const& path) -> std::string {
    auto input = std::ifstream{path, std::ios::binary};
    if (!input)
        return {};
    return std::string{
        std::istreambuf_iterator<char>{input},
        std::istreambuf_iterator<char>{}};
}

auto path_exists(std::filesystem::path const& path) -> bool {
    auto ec = std::error_code{};
    return std::filesystem::exists(path, ec);
}

auto path_is_directory(std::filesystem::path const& path) -> bool {
    auto ec = std::error_code{};
    return std::filesystem::is_directory(path, ec);
}

auto file_size_or_zero(std::filesystem::path const& path) -> std::uintmax_t {
    auto ec = std::error_code{};
    auto size = std::filesystem::file_size(path, ec);
    return ec ? 0 : size;
}

auto safe_relative_path(std::filesystem::path const& path) -> bool {
    if (path.empty() || path.is_absolute())
        return false;
    for (auto const& part : path.lexically_normal()) {
        if (part == "..")
            return false;
    }
    return true;
}

auto path_stays_under_root(std::filesystem::path const& root,
                           std::filesystem::path const& path,
                           std::string& error) -> bool {
    auto ec = std::error_code{};
    auto canonical_root = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        error = ec.message();
        return false;
    }
    auto canonical_path = std::filesystem::weakly_canonical(path, ec);
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

auto count_regular_files(std::filesystem::path const& root) -> std::size_t {
    if (!path_is_directory(root))
        return 0;

    auto count = std::size_t{0};
    auto ec = std::error_code{};
    auto options = std::filesystem::directory_options::skip_permission_denied;
    for (auto it = std::filesystem::recursive_directory_iterator(
             root,
             options,
             ec);
         !ec && it != std::filesystem::recursive_directory_iterator{};
         it.increment(ec)) {
        if (it->is_regular_file(ec))
            ++count;
    }
    return count;
}

auto output_tail(std::string_view text, std::size_t max_bytes = 16384)
        -> std::string_view {
    if (text.size() <= max_bytes)
        return text;
    return text.substr(text.size() - max_bytes);
}

auto executable_filename(std::string const& package_name) -> std::string {
#if defined(_WIN32)
    return package_name + ".exe";
#else
    return package_name;
#endif
}

auto json_object_member(json::Object const& object, std::string_view key)
    -> json::Value const* {
    auto found = object.find(std::string{key});
    return found == object.end() ? nullptr : &found->second;
}

auto json_at(json::Value const& value,
             std::initializer_list<std::string_view> path)
    -> json::Value const* {
    auto current = &value;
    for (auto key : path) {
        if (!current || !current->is_object())
            return nullptr;
        current = json_object_member(current->as_object(), key);
    }
    return current;
}

auto json_array_at(json::Value const& value,
                   std::initializer_list<std::string_view> path)
    -> json::Array const* {
    auto const* found = json_at(value, path);
    return found && found->is_array() ? &found->as_array() : nullptr;
}

auto json_object_at(json::Value const& value,
                    std::initializer_list<std::string_view> path)
    -> json::Object const* {
    auto const* found = json_at(value, path);
    return found && found->is_object() ? &found->as_object() : nullptr;
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

auto write_text_file(std::filesystem::path const& path,
                     std::string_view content,
                     std::string& error) -> bool {
    auto ec = std::error_code{};
    auto parent = path.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, ec);
        if (ec) {
            error = ec.message();
            return false;
        }
    }
    auto output = std::ofstream{path, std::ios::binary};
    if (!output) {
        error = "could not open output file";
        return false;
    }
    output << content;
    if (!output) {
        error = "could not write output file";
        return false;
    }
    return true;
}

auto single_positional_text_or_error(cppx::cli::Invocation const& invocation,
                                     std::string_view command,
                                     std::string_view value_name)
        -> std::expected<std::string, std::string> {
    if (invocation.positionals.empty()) {
        return std::unexpected{
            std::format("{} requires one positional {}", command, value_name)};
    }
    if (invocation.positionals.size() > 1) {
        return std::unexpected{
            std::format(
                "{} accepts exactly one positional {}",
                command,
                value_name)};
    }
    return invocation.positionals.front();
}

auto error_json(std::string_view command, std::string_view message)
        -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":{},\"ok\":false,\"error\":{}}}",
        json_string(command),
        json_string(message));
}

int print_error(std::string_view command, std::string_view message, bool json) {
    if (json) {
        std::println("{}", error_json(command, message));
    } else {
        std::println(
            std::cerr,
            "{}",
            cppx::terminal::format_diagnostic({
                .severity = cppx::terminal::DiagnosticSeverity::error,
                .message = std::string{message},
                .context = std::string{command},
            }));
    }
    return 2;
}

auto all_ok(std::span<Check const> checks) -> bool {
    return std::ranges::all_of(checks, [](Check const& check) {
        return check.ok;
    });
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

auto string_view_array_json(std::span<std::string_view const> values)
        -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(values[i]);
    }
    out += "]";
    return out;
}

auto check_json(Check const& check) -> std::string {
    return std::format(
        "{{\"name\":{},\"ok\":{},\"detail\":{},\"hint\":{}}}",
        json_string(check.name),
        check.ok ? "true" : "false",
        json_string(check.detail),
        json_string(check.ok ? "" : check.hint));
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

auto resource_diagnostic_json(
        phenotype::ResourceDiagnostic const& diagnostic) -> std::string {
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

}

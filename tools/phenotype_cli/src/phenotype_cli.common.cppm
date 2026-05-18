export module phenotype_cli.common;

import cppx.terminal;
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

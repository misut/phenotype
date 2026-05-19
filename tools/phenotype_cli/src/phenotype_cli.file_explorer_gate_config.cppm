export module phenotype_cli.file_explorer_gate_config;

import cppx.cli;
import phenotype_cli.common;
import phenotype_cli.runtime;
import std;

export namespace phenotype_cli::file_explorer_gate {

using namespace phenotype_cli::common;
using phenotype_cli::runtime::environment_value;

void append_unique(std::vector<std::string>& values, std::string value) {
    if (std::ranges::find(values, value) == values.end())
        values.push_back(std::move(value));
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
        "svg-selected",
        "history-forward",
        "sorted-kind",
        "search-active",
        "preferences-panel",
    };
}

auto normalize_file_explorer_scenario_name(std::string scenario)
        -> std::string {
    return scenario == "startup" ? std::string{"default"} : std::move(scenario);
}

auto normalize_file_explorer_scenarios(std::vector<std::string> scenarios)
        -> std::vector<std::string> {
    auto out = std::vector<std::string>{};
    out.reserve(scenarios.size());
    for (auto& scenario : scenarios)
        append_unique(out, normalize_file_explorer_scenario_name(
                           std::move(scenario)));
    return out;
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

} // namespace phenotype_cli::file_explorer_gate

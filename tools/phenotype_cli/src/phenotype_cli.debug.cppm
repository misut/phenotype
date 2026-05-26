export module phenotype_cli.debug;

import cppx.cli;
import cppx.terminal;
import phenotype.io;
import phenotype_cli.common;
import std;

namespace phenotype_cli::debug {

namespace io_contract = phenotype::io;
using namespace phenotype_cli::common;

auto debug_domains_json() -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < io_contract::debug_protocol_domains.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(io_contract::debug_protocol_domain_name(
            io_contract::debug_protocol_domains[i]));
    }
    out += "]";
    return out;
}

auto debug_commands_json() -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < io_contract::debug_protocol_commands.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(io_contract::debug_protocol_command_name(
            io_contract::debug_protocol_commands[i]));
    }
    out += "]";
    return out;
}

auto debug_panel_sections_json() -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < io_contract::debug_panel_sections.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(io_contract::debug_panel_section_name(
            io_contract::debug_panel_sections[i]));
    }
    out += "]";
    return out;
}

auto debug_contract_checks() -> std::vector<Check> {
    return {
        {.name = "protocol_domains",
         .ok = io_contract::debug_protocol_domains.size() >= 9,
         .detail = std::format(
             "domains={}",
             io_contract::debug_protocol_domains.size()),
         .hint =
             "Debug sessions need session, target, input, layout, overlay, console, performance, artifact, and replay domains."},
        {.name = "agent_commands",
         .ok = io_contract::debug_protocol_commands.size() >= 15,
         .detail = std::format(
             "commands={}",
             io_contract::debug_protocol_commands.size()),
         .hint =
             "CLI and panel clients need launch/attach, input, layout, overlay, console, performance, trace, replay, and artifact commands."},
        {.name = "side_panel_sections",
         .ok = io_contract::debug_panel_sections.size() >= 7,
         .detail = std::format(
             "sections={}",
             io_contract::debug_panel_sections.size()),
         .hint =
             "The side panel should expose summary, elements, input, console, performance, layout, and trace/replay views."},
        {.name = "transport_policy",
         .ok = io_contract::debug_transport_policy().find("json_commands")
                != std::string_view::npos,
         .detail = std::string{io_contract::debug_transport_policy()},
         .hint =
             "The debug endpoint must be protocol-first so CLI, side panel, and agents share one command/event surface."},
        {.name = "security_policy",
         .ok = io_contract::debug_security_policy().find("local_only")
                != std::string_view::npos,
         .detail = std::string{io_contract::debug_security_policy()},
         .hint =
             "A controllable debug endpoint must stay local-only and gated in debug builds."},
        {.name = "agent_control",
         .ok = io_contract::debug_agent_control_policy().find("semantic_targets")
                != std::string_view::npos,
         .detail = std::string{io_contract::debug_agent_control_policy()},
         .hint =
             "Agents should drive semantic targets first and fall back to coordinates for raw pointer coverage."},
    };
}

auto debug_contract_json(std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"debug contract\","
        "\"ok\":{},\"debug_protocol_version\":{},"
        "\"policies\":{{\"transport\":{},\"security\":{},"
        "\"side_panel\":{},\"agent_control\":{}}},"
        "\"domains\":{},\"commands\":{},\"panel_sections\":{},"
        "\"shortcuts\":{{\"macos\":\"super+f12\",\"windows\":\"control+f12\"}},"
        "\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        io_contract::debug_protocol_contract_version,
        json_string(io_contract::debug_transport_policy()),
        json_string(io_contract::debug_security_policy()),
        json_string(io_contract::debug_side_panel_policy()),
        json_string(io_contract::debug_agent_control_policy()),
        debug_domains_json(),
        debug_commands_json(),
        debug_panel_sections_json(),
        checks_json(checks));
}

export int run_debug_contract(cppx::cli::Invocation const& invocation) {
    auto checks = debug_contract_checks();
    if (invocation.has("json")) {
        std::println("{}", debug_contract_json(checks));
        return all_ok(checks) ? 0 : 1;
    }

    auto lines = std::vector<cppx::terminal::StatusLine>{
        {.label = "version",
         .value = std::format(
             "{}",
             io_contract::debug_protocol_contract_version),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "domains",
         .value = std::format(
             "{}",
             io_contract::debug_protocol_domains.size()),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "commands",
         .value = std::format(
             "{}",
             io_contract::debug_protocol_commands.size()),
         .status = cppx::terminal::StatusKind::ok},
        {.label = "panel",
         .value = std::string{io_contract::debug_side_panel_policy()},
         .status = cppx::terminal::StatusKind::ok},
        {.label = "shortcut",
         .value = "macos=super+f12 windows=control+f12",
         .status = cppx::terminal::StatusKind::ok},
        {.label = "security",
         .value = std::string{io_contract::debug_security_policy()},
         .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                  : cppx::terminal::StatusKind::fail},
    };
    std::println("phenotype debug contract");
    std::println("{}", cppx::terminal::format_status_frame(lines, false));
    print_checks("checks", checks);
    return all_ok(checks) ? 0 : 1;
}

} // namespace phenotype_cli::debug

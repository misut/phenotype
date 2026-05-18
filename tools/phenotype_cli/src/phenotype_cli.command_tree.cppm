export module phenotype_cli.command_tree;

import cppx.cli;
import phenotype_cli.common;
import std;

export namespace phenotype_cli::command_tree {

using namespace phenotype_cli::common;

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

} // namespace phenotype_cli::command_tree

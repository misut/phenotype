#include <cassert>
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <string_view>

import json;
import phenotype_cli.command_tree;
import phenotype_cli.commands;
import phenotype_cli.common;
import std;

namespace {

using phenotype_cli::common::json_array_at;
using phenotype_cli::common::json_bool_at;
using phenotype_cli::common::json_string_at;

auto path_matches(
        json::Value const& command,
        std::initializer_list<std::string_view> expected) -> bool {
    auto const* path = json_array_at(command, {"path"});
    if (!path || path->size() != expected.size())
        return false;

    auto expected_part = expected.begin();
    for (auto const& actual_part : *path) {
        if (!actual_part.is_string()
            || actual_part.as_string() != std::string{*expected_part}) {
            return false;
        }
        ++expected_part;
    }
    return true;
}

auto find_command(
        json::Value const& tree,
        std::initializer_list<std::string_view> path) -> json::Value const& {
    auto const* commands = json_array_at(tree, {"commands"});
    assert(commands);
    for (auto const& command : *commands) {
        if (path_matches(command, path))
            return command;
    }
    std::abort();
}

auto find_option(json::Value const& command, std::string_view name)
        -> json::Value const& {
    auto const* options = json_array_at(command, {"spec", "options"});
    assert(options);
    for (auto const& option : *options) {
        auto actual_name = json_string_at(option, {"name"});
        if (actual_name && *actual_name == name)
            return option;
    }
    std::abort();
}

void assert_bound_option(json::Value const& command, std::string_view name) {
    auto const& option = find_option(command, name);

    auto arity = json_string_at(option, {"arity"});
    assert(arity && *arity == "one");

    auto repeatable = json_bool_at(option, {"repeatable"});
    assert(repeatable && *repeatable);

    auto value_name = json_string_at(option, {"value_name"});
    assert(value_name && *value_name == "key=json");
}

void assert_direct_material_bound_options(json::Value const& command) {
    assert_bound_option(command, "require-material-budget-bound");
    assert_bound_option(command, "require-material-resource-bound");
    assert_bound_option(command, "require-material-quality-bound");
}

} // namespace

int main() {
    auto tree = json::parse(phenotype_cli::command_tree::command_tree_json(
        phenotype_cli::commands::spec()));

    assert_direct_material_bound_options(find_command(
        tree,
        {"phenotype", "artifact", "verify-glass-showcase"}));
    assert_direct_material_bound_options(find_command(
        tree,
        {"phenotype", "artifact", "verify-file-explorer"}));
}

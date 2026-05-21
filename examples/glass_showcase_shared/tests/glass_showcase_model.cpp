#include <cassert>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string>
#include <vector>

import glass_showcase_shared;

namespace {

[[noreturn]] void fail_assert(char const* expression, char const* file, int line) {
    std::cerr << "FAIL: " << file << ":" << line << ": " << expression << "\n";
    std::exit(1);
}

} // namespace

#undef assert
#define assert(expression) \
    ((expression) ? static_cast<void>(0) : fail_assert(#expression, __FILE__, __LINE__))

int main() {
    namespace demo = glass_showcase_demo;

    auto parsed_density = demo::parse_glass_input("density:dense");
    assert(parsed_density.ok);
    assert(parsed_density.input.kind == demo::GlassInputKind::SelectDensity);
    assert(parsed_density.input.density == 2);

    auto parsed_backdrop = demo::parse_glass_input("backdrop:high");
    assert(parsed_backdrop.ok);
    assert(parsed_backdrop.input.kind == demo::GlassInputKind::SetBackdrop);
    assert(parsed_backdrop.input.flag);

    auto parsed_inspector = demo::parse_glass_input("inspector:closed");
    assert(parsed_inspector.ok);
    assert(parsed_inspector.input.kind == demo::GlassInputKind::SetInspector);
    assert(!parsed_inspector.input.flag);

    auto parsed_viewport = demo::parse_glass_input("viewport:640x820@2");
    assert(parsed_viewport.ok);
    assert(parsed_viewport.input.kind == demo::GlassInputKind::Viewport);
    assert(parsed_viewport.input.viewport_width == 640);
    assert(parsed_viewport.input.viewport_height == 820);
    assert(parsed_viewport.input.viewport_scale == 2.0f);

    auto bad_density = demo::parse_glass_input("density:wide");
    assert(!bad_density.ok);
    assert(bad_density.error.find("density") != std::string::npos);

    auto material_count =
        demo::parse_glass_expectation("material-count:11");
    assert(material_count.ok);
    assert(material_count.expectation.kind
        == demo::GlassExpectationKind::MaterialCount);

    auto inputs = std::vector<demo::GlassInput>{
        demo::parse_glass_input("backdrop:high").input,
        demo::parse_glass_input("density:comfortable").input,
        demo::parse_glass_input("inspector:closed").input,
        demo::parse_glass_input("note:CLI observable material state").input,
        demo::parse_glass_input("viewport:640x820@2").input,
    };
    auto result = demo::drive_glass_showcase(inputs);
    assert(result.trace.size() == inputs.size());
    assert(result.state.high_contrast_backdrop);
    assert(!result.state.inspector_open);
    assert(result.state.selected_density == 0);
    assert(result.state.note == "CLI observable material state");
    assert(result.state.viewport_width == 640);
    assert(result.state.viewport_height == 820);
    assert(result.state.viewport_scale == 2.0f);
    assert(demo::expected_material_plan_count(result.state) == 11);
    assert(demo::expected_material_probe_count(result.state) == 11);
    assert(demo::progress_value(result.state) == 0.85f);
    auto closed_contract = demo::glass_probe_contract(result.state);
    assert(closed_contract.active_material_probe_count == 11);
    assert(closed_contract.total_expected_execution_stages == 44);
    assert(closed_contract.sample_taps_per_probe == 25);
    assert(closed_contract.max_blur_radius == 36.0f);
    auto checkbox_probe = demo::glass_material_probe_at(5);
    assert(checkbox_probe.name == std::string_view{"checkbox_control_probe"});
    assert(checkbox_probe.interactive);
    assert(checkbox_probe.likely_layer
           == std::string_view{"material-control-indicator"});
    auto radio_probe = demo::glass_material_probe_at(6);
    assert(radio_probe.name == std::string_view{"radio_control_probe"});
    assert(radio_probe.interactive);
    auto switch_probe = demo::glass_material_probe_at(7);
    assert(switch_probe.name == std::string_view{"switch_control_probe"});
    assert(switch_probe.interactive);
    assert(switch_probe.likely_layer
           == std::string_view{"material-control-track"});
    auto blur_probe = demo::glass_material_probe_at(8);
    assert(blur_probe.name == std::string_view{"visible_blur_probe"});
    assert(blur_probe.has_union_id);
    assert(blur_probe.union_id == 1u);
    assert(blur_probe.expected_pass == std::string_view{"backdrop-sample-blur"});
    assert(blur_probe.requires_edge_highlight);
    assert(blur_probe.excludes_foreground_text);
    assert(demo::glass_probe_material_kind_name(blur_probe.kind)
           == std::string_view{"thin"});

    auto expectations = std::vector<demo::GlassExpectation>{
        demo::parse_glass_expectation("backdrop:high").expectation,
        demo::parse_glass_expectation("density:comfortable").expectation,
        demo::parse_glass_expectation("inspector:closed").expectation,
        demo::parse_glass_expectation("note-contains:observable").expectation,
        demo::parse_glass_expectation("viewport:640x820@2").expectation,
        demo::parse_glass_expectation("material-count:11").expectation,
    };
    auto checked = demo::check_glass_expectations(result, expectations);
    assert(demo::glass_expectations_ok(checked));

    auto reset_input = demo::parse_glass_input("reset");
    assert(reset_input.ok);
    auto reset_result = demo::drive_glass_showcase(
        std::span<demo::GlassInput const>{&reset_input.input, 1});
    assert(reset_result.state.inspector_open);
    assert(reset_result.state.selected_density == demo::k_default_density);
    assert(demo::expected_material_plan_count(reset_result.state) == 12);
    auto open_contract = demo::glass_probe_contract(reset_result.state);
    assert(open_contract.active_material_probe_count == 12);
    assert(open_contract.total_expected_execution_stages == 48);
    auto tooltip_probe = demo::glass_material_probe_at(9);
    assert(tooltip_probe.name == std::string_view{"tooltip_probe"});
    assert(!tooltip_probe.interactive);
    auto context_menu_probe = demo::glass_material_probe_at(10);
    assert(context_menu_probe.name == std::string_view{"context_menu_probe"});
    assert(context_menu_probe.interactive);
    auto debug_probe = demo::glass_material_probe_at(11);
    assert(debug_probe.requires_inspector_open);
    assert(demo::glass_probe_is_active(debug_probe, reset_result.state));
    assert(!demo::glass_probe_is_active(debug_probe, result.state));

    auto debug_payload =
        demo::glass_showcase_application_debug_json(reset_result.state);
    auto& application = debug_payload.as_object();
    assert(application.contains("glass_showcase"));
    auto& showcase = application["glass_showcase"].as_object();
    assert(showcase["kind"].as_string() == "glass_showcase");
    auto& contract = showcase["probe_contract"].as_object();
    assert(contract["contract_name"].as_string()
           == "glass_showcase_material_probe_contract");
    assert(contract["active_material_probe_count"].as_integer() == 12);
    auto& probes = contract["material_probes"].as_object();
    assert(probes.contains("visible_blur_probe"));
    assert(probes.contains("checkbox_control_probe"));
    assert(probes.contains("radio_control_probe"));
    assert(probes.contains("switch_control_probe"));
    assert(probes.contains("tooltip_probe"));
    assert(probes.contains("context_menu_probe"));
    assert(probes["visible_blur_probe"].as_object()["kind"].as_string()
           == "thin");
    assert(probes["context_menu_probe"].as_object()["interactive"].as_bool());
}

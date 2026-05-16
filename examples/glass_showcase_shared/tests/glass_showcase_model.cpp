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
        demo::parse_glass_expectation("material-count:6");
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
    assert(demo::expected_material_plan_count(result.state) == 6);
    assert(demo::progress_value(result.state) == 0.85f);

    auto expectations = std::vector<demo::GlassExpectation>{
        demo::parse_glass_expectation("backdrop:high").expectation,
        demo::parse_glass_expectation("density:comfortable").expectation,
        demo::parse_glass_expectation("inspector:closed").expectation,
        demo::parse_glass_expectation("note-contains:observable").expectation,
        demo::parse_glass_expectation("viewport:640x820@2").expectation,
        demo::parse_glass_expectation("material-count:6").expectation,
    };
    auto checked = demo::check_glass_expectations(result, expectations);
    assert(demo::glass_expectations_ok(checked));

    auto reset_input = demo::parse_glass_input("reset");
    assert(reset_input.ok);
    auto reset_result = demo::drive_glass_showcase(
        std::span<demo::GlassInput const>{&reset_input.input, 1});
    assert(reset_result.state.inspector_open);
    assert(reset_result.state.selected_density == demo::k_default_density);
    assert(demo::expected_material_plan_count(reset_result.state) == 7);
}

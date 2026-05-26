#include <cassert>
#include <new>
#include <cstdio>
#include <string_view>

import phenotype.io;

namespace io = phenotype::io;

int main() {
    assert(io::io_contract_version == 1);
    assert(io::debug_protocol_contract_version == 1);

    assert(io::input_event_kinds.size() == 7);
    assert(io::input_event_kind_name(io::InputEventKind::Pointer) == "pointer");
    assert(io::input_event_kind_name(io::InputEventKind::Key) == "key");
    assert(io::input_event_kind_name(io::InputEventKind::Text) == "text");
    assert(io::input_event_kind_name(io::InputEventKind::Scroll) == "scroll");
    assert(io::input_event_kind_name(io::InputEventKind::Viewport) == "viewport");
    assert(io::input_event_kind_name(io::InputEventKind::Tick) == "tick");
    assert(io::input_event_kind_name(io::InputEventKind::Capability) == "capability");

    io::InputFrame frame{
        .frame_index = 1,
        .timestamp_ms = 16,
        .deterministic = true,
    };
    frame.events.push_back(io::InputEvent{
        .sequence = 1,
        .payload = io::InputViewportEvent{
            .width = 900,
            .height = 620,
            .scale = 2.0f,
        },
    });
    frame.events.push_back(io::InputEvent{
        .sequence = 2,
        .payload = io::InputPointerEvent{
            .phase = io::PointerPhase::Down,
            .pointer_id = 7,
            .x = 120.0f,
            .y = 44.0f,
            .buttons = 1,
        },
    });
    frame.events.push_back(io::InputEvent{
        .sequence = 0,
        .payload = io::InputTickEvent{.elapsed_ms = 16},
    });

    assert(io::input_frame_event_count(frame) == 3);
    assert(io::input_event_kind(frame.events[0]) == io::InputEventKind::Viewport);
    assert(io::input_event_kind(frame.events[1]) == io::InputEventKind::Pointer);
    assert(io::input_event_kind(frame.events[2]) == io::InputEventKind::Tick);
    assert(io::input_frame_is_replayable(frame));

    io::InputScript script{
        .source_name = "io-contract-test",
        .deterministic = true,
    };
    script.frames.push_back(frame);
    assert(io::input_script_event_count(script) == 3);
    assert(io::input_script_is_replayable(script));

    io::InputFrame unstable_frame = frame;
    unstable_frame.events[1].sequence = 0;
    assert(!io::input_frame_is_replayable(unstable_frame));

    io::InputScript unstable_script = script;
    unstable_script.deterministic = false;
    assert(!io::input_script_is_replayable(unstable_script));

    assert(io::output_observation_kinds.size() == 6);
    assert(io::output_observation_kind_name(io::OutputObservationKind::SemanticTree)
           == "semantic_tree");
    assert(io::output_observation_kind_name(io::OutputObservationKind::ArtifactBundle)
           == "artifact_bundle");

    assert(io::debug_protocol_domains.size() == 9);
    assert(io::debug_protocol_domain_name(io::DebugProtocolDomain::Input)
           == "input");
    assert(io::debug_protocol_domain_name(io::DebugProtocolDomain::Layout)
           == "layout");
    assert(io::debug_protocol_domain_name(io::DebugProtocolDomain::Performance)
           == "performance");
    assert(io::debug_protocol_commands.size() == 15);
    assert(io::debug_protocol_command_name(io::DebugProtocolCommand::SendInput)
           == "send_input");
    assert(io::debug_protocol_command_name(io::DebugProtocolCommand::HighlightNode)
           == "highlight_node");
    assert(io::debug_protocol_command_name(io::DebugProtocolCommand::ReplayScript)
           == "replay_script");
    assert(io::debug_panel_sections.size() == 7);
    assert(io::debug_panel_section_name(io::DebugPanelSection::Console)
           == "console");

    io::OutputObservation observation{
        .semantic_tree_present = true,
        .command_stream_present = true,
        .material_plans_present = true,
        .runtime_summary_present = true,
        .machine_readable_failure_shape = true,
        .semantic_node_count = 12,
        .command_stream = {
            .command_count = 24,
            .path_count = 5,
            .material_count = 3,
            .text_count = 8,
            .image_count = 1,
            .bounded = true,
        },
        .material = {
            .plan_count = 3,
            .fallback_count = 1,
            .runtime_pass_count = 4,
            .semantic_runtime_match = true,
        },
    };
    observation.pixel_regions.push_back(io::OutputPixelRegionSummary{
        .name = "toolbar_glass_probe",
        .luma_delta = 0.18f,
        .sampled_pixels = 128,
        .unique_colors = 22,
        .likely_layer = "material_surface",
        .likely_pass = "backdrop_blur",
    });
    observation.likely_layers.push_back("material_surface");
    observation.likely_passes.push_back("backdrop_blur");
    assert(io::output_observation_is_llm_debuggable(observation));

    io::ArtifactBundleDescriptor bundle{
        .snapshot_json = true,
        .frame_image = true,
        .platform_runtime_details = true,
        .observation = observation,
    };
    assert(io::artifact_bundle_is_llm_debuggable(bundle));

    bundle.platform_runtime_details = false;
    assert(!io::artifact_bundle_is_llm_debuggable(bundle));

    assert(io::input_contract_policy().find("typed_input_frames")
           != std::string_view::npos);
    assert(io::output_contract_policy().find("typed_observations")
           != std::string_view::npos);
    assert(io::edge_effect_policy().find("device")
           != std::string_view::npos);
    assert(io::production_bypass_policy().find("release_adapters")
           != std::string_view::npos);
    assert(io::debug_transport_policy().find("json_commands")
           != std::string_view::npos);
    assert(io::debug_security_policy().find("local_only")
           != std::string_view::npos);
    assert(io::debug_side_panel_policy().find("same_debug_protocol")
           != std::string_view::npos);
    assert(io::debug_agent_control_policy().find("semantic_targets")
           != std::string_view::npos);
    assert(io::debug_protocol_is_agent_complete());

    std::puts("PASS: pure phenotype.io contract");
}

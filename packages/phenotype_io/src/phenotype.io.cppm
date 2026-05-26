module;
#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

export module phenotype.io;

export namespace phenotype::io {

inline constexpr std::uint32_t io_contract_version = 1;
inline constexpr std::uint32_t debug_protocol_contract_version = 1;

enum class InputEventKind {
    Pointer,
    Key,
    Text,
    Scroll,
    Viewport,
    Tick,
    Capability,
};

inline constexpr std::array input_event_kinds = {
    InputEventKind::Pointer,
    InputEventKind::Key,
    InputEventKind::Text,
    InputEventKind::Scroll,
    InputEventKind::Viewport,
    InputEventKind::Tick,
    InputEventKind::Capability,
};

enum class PointerPhase {
    Down,
    Move,
    Up,
    Cancel,
    Hover,
};

enum class KeyPhase {
    Down,
    Up,
};

struct InputPointerEvent {
    PointerPhase phase = PointerPhase::Move;
    std::uint32_t pointer_id = 0;
    float x = 0.0f;
    float y = 0.0f;
    std::uint32_t buttons = 0;
};

struct InputKeyEvent {
    KeyPhase phase = KeyPhase::Down;
    std::string key;
    std::string shortcut;
    bool repeat = false;
};

struct InputTextEvent {
    std::string text;
};

struct InputScrollEvent {
    float delta_x = 0.0f;
    float delta_y = 0.0f;
    bool precise = true;
};

struct InputViewportEvent {
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    float scale = 1.0f;
};

struct InputTickEvent {
    std::int64_t elapsed_ms = 0;
};

struct InputCapabilityEvent {
    bool material_surfaces = false;
    bool backdrop_blur = false;
    bool frame_capture = false;
    bool write_artifact_bundle = false;
};

using InputEventPayload = std::variant<
    InputPointerEvent,
    InputKeyEvent,
    InputTextEvent,
    InputScrollEvent,
    InputViewportEvent,
    InputTickEvent,
    InputCapabilityEvent>;

struct InputEvent {
    std::uint64_t sequence = 0;
    InputEventPayload payload = InputTickEvent{};
};

struct InputFrame {
    std::uint64_t frame_index = 0;
    std::int64_t timestamp_ms = 0;
    bool deterministic = true;
    std::vector<InputEvent> events;
};

struct InputScript {
    std::string source_name;
    bool deterministic = true;
    std::vector<InputFrame> frames;
};

enum class OutputObservationKind {
    SemanticTree,
    CommandStream,
    MaterialPlan,
    RuntimeSummary,
    PixelRegion,
    ArtifactBundle,
};

inline constexpr std::array output_observation_kinds = {
    OutputObservationKind::SemanticTree,
    OutputObservationKind::CommandStream,
    OutputObservationKind::MaterialPlan,
    OutputObservationKind::RuntimeSummary,
    OutputObservationKind::PixelRegion,
    OutputObservationKind::ArtifactBundle,
};

struct OutputCommandStreamSummary {
    std::uint32_t command_count = 0;
    std::uint32_t path_count = 0;
    std::uint32_t material_count = 0;
    std::uint32_t text_count = 0;
    std::uint32_t image_count = 0;
    bool bounded = true;
};

struct OutputMaterialSummary {
    std::uint32_t plan_count = 0;
    std::uint32_t fallback_count = 0;
    std::uint32_t runtime_pass_count = 0;
    bool semantic_runtime_match = true;
};

struct OutputPixelRegionSummary {
    std::string name;
    float luma_delta = 0.0f;
    std::uint32_t sampled_pixels = 0;
    std::uint32_t unique_colors = 0;
    std::string likely_layer;
    std::string likely_pass;
};

struct OutputObservation {
    bool semantic_tree_present = false;
    bool command_stream_present = false;
    bool material_plans_present = false;
    bool runtime_summary_present = false;
    bool machine_readable_failure_shape = false;
    std::uint32_t semantic_node_count = 0;
    OutputCommandStreamSummary command_stream;
    OutputMaterialSummary material;
    std::vector<OutputPixelRegionSummary> pixel_regions;
    std::vector<std::string> likely_layers;
    std::vector<std::string> likely_passes;
};

struct ArtifactBundleDescriptor {
    bool snapshot_json = false;
    bool frame_image = false;
    bool platform_runtime_details = false;
    OutputObservation observation;
};

enum class DebugProtocolDomain {
    Session,
    Target,
    Input,
    Layout,
    Overlay,
    Console,
    Performance,
    Artifact,
    TraceReplay,
};

inline constexpr std::array debug_protocol_domains = {
    DebugProtocolDomain::Session,
    DebugProtocolDomain::Target,
    DebugProtocolDomain::Input,
    DebugProtocolDomain::Layout,
    DebugProtocolDomain::Overlay,
    DebugProtocolDomain::Console,
    DebugProtocolDomain::Performance,
    DebugProtocolDomain::Artifact,
    DebugProtocolDomain::TraceReplay,
};

enum class DebugProtocolCommand {
    Launch,
    Attach,
    Detach,
    Snapshot,
    Subscribe,
    SendInput,
    QueryLayout,
    HighlightNode,
    SetOverlay,
    ReadConsole,
    ReadPerformance,
    StartTrace,
    StopTrace,
    ReplayScript,
    WriteArtifact,
};

inline constexpr std::array debug_protocol_commands = {
    DebugProtocolCommand::Launch,
    DebugProtocolCommand::Attach,
    DebugProtocolCommand::Detach,
    DebugProtocolCommand::Snapshot,
    DebugProtocolCommand::Subscribe,
    DebugProtocolCommand::SendInput,
    DebugProtocolCommand::QueryLayout,
    DebugProtocolCommand::HighlightNode,
    DebugProtocolCommand::SetOverlay,
    DebugProtocolCommand::ReadConsole,
    DebugProtocolCommand::ReadPerformance,
    DebugProtocolCommand::StartTrace,
    DebugProtocolCommand::StopTrace,
    DebugProtocolCommand::ReplayScript,
    DebugProtocolCommand::WriteArtifact,
};

enum class DebugPanelSection {
    Summary,
    Elements,
    Input,
    Console,
    Performance,
    Layout,
    TraceReplay,
};

inline constexpr std::array debug_panel_sections = {
    DebugPanelSection::Summary,
    DebugPanelSection::Elements,
    DebugPanelSection::Input,
    DebugPanelSection::Console,
    DebugPanelSection::Performance,
    DebugPanelSection::Layout,
    DebugPanelSection::TraceReplay,
};

inline auto input_event_kind_name(InputEventKind kind) noexcept
        -> std::string_view {
    switch (kind) {
    case InputEventKind::Pointer:    return "pointer";
    case InputEventKind::Key:        return "key";
    case InputEventKind::Text:       return "text";
    case InputEventKind::Scroll:     return "scroll";
    case InputEventKind::Viewport:   return "viewport";
    case InputEventKind::Tick:       return "tick";
    case InputEventKind::Capability: return "capability";
    }
    return "tick";
}

inline auto input_event_kind(InputEvent const& event) noexcept
        -> InputEventKind {
    return std::visit([](auto const& payload) noexcept {
        using T = std::decay_t<decltype(payload)>;
        if constexpr (std::is_same_v<T, InputPointerEvent>)
            return InputEventKind::Pointer;
        else if constexpr (std::is_same_v<T, InputKeyEvent>)
            return InputEventKind::Key;
        else if constexpr (std::is_same_v<T, InputTextEvent>)
            return InputEventKind::Text;
        else if constexpr (std::is_same_v<T, InputScrollEvent>)
            return InputEventKind::Scroll;
        else if constexpr (std::is_same_v<T, InputViewportEvent>)
            return InputEventKind::Viewport;
        else if constexpr (std::is_same_v<T, InputCapabilityEvent>)
            return InputEventKind::Capability;
        else
            return InputEventKind::Tick;
    }, event.payload);
}

inline auto output_observation_kind_name(OutputObservationKind kind) noexcept
        -> std::string_view {
    switch (kind) {
    case OutputObservationKind::SemanticTree:   return "semantic_tree";
    case OutputObservationKind::CommandStream:  return "command_stream";
    case OutputObservationKind::MaterialPlan:   return "material_plan";
    case OutputObservationKind::RuntimeSummary: return "runtime_summary";
    case OutputObservationKind::PixelRegion:    return "pixel_region";
    case OutputObservationKind::ArtifactBundle: return "artifact_bundle";
    }
    return "artifact_bundle";
}

inline auto debug_protocol_domain_name(DebugProtocolDomain domain) noexcept
        -> std::string_view {
    switch (domain) {
    case DebugProtocolDomain::Session:     return "session";
    case DebugProtocolDomain::Target:      return "target";
    case DebugProtocolDomain::Input:       return "input";
    case DebugProtocolDomain::Layout:      return "layout";
    case DebugProtocolDomain::Overlay:     return "overlay";
    case DebugProtocolDomain::Console:     return "console";
    case DebugProtocolDomain::Performance: return "performance";
    case DebugProtocolDomain::Artifact:    return "artifact";
    case DebugProtocolDomain::TraceReplay: return "trace_replay";
    }
    return "session";
}

inline auto debug_protocol_command_name(DebugProtocolCommand command) noexcept
        -> std::string_view {
    switch (command) {
    case DebugProtocolCommand::Launch:          return "launch";
    case DebugProtocolCommand::Attach:          return "attach";
    case DebugProtocolCommand::Detach:          return "detach";
    case DebugProtocolCommand::Snapshot:        return "snapshot";
    case DebugProtocolCommand::Subscribe:       return "subscribe";
    case DebugProtocolCommand::SendInput:       return "send_input";
    case DebugProtocolCommand::QueryLayout:     return "query_layout";
    case DebugProtocolCommand::HighlightNode:   return "highlight_node";
    case DebugProtocolCommand::SetOverlay:      return "set_overlay";
    case DebugProtocolCommand::ReadConsole:     return "read_console";
    case DebugProtocolCommand::ReadPerformance: return "read_performance";
    case DebugProtocolCommand::StartTrace:      return "start_trace";
    case DebugProtocolCommand::StopTrace:       return "stop_trace";
    case DebugProtocolCommand::ReplayScript:    return "replay_script";
    case DebugProtocolCommand::WriteArtifact:   return "write_artifact";
    }
    return "snapshot";
}

inline auto debug_panel_section_name(DebugPanelSection section) noexcept
        -> std::string_view {
    switch (section) {
    case DebugPanelSection::Summary:     return "summary";
    case DebugPanelSection::Elements:    return "elements";
    case DebugPanelSection::Input:       return "input";
    case DebugPanelSection::Console:     return "console";
    case DebugPanelSection::Performance: return "performance";
    case DebugPanelSection::Layout:      return "layout";
    case DebugPanelSection::TraceReplay: return "trace_replay";
    }
    return "summary";
}

inline auto input_contract_policy() noexcept -> std::string_view {
    return "platform_edge_events_lower_to_typed_input_frames";
}

inline auto output_contract_policy() noexcept -> std::string_view {
    return "renderer_outputs_lower_to_typed_observations";
}

inline auto edge_effect_policy() noexcept -> std::string_view {
    return "filesystem_clock_process_capture_shader_and_device_effects_stay_at_edge";
}

inline auto production_bypass_policy() noexcept -> std::string_view {
    return "release_adapters_may_bypass_debug_serialization_but_keep_value_contract";
}

inline auto debug_transport_policy() noexcept -> std::string_view {
    return "debug_session_uses_versioned_json_commands_and_events";
}

inline auto debug_security_policy() noexcept -> std::string_view {
    return "debug_endpoint_is_debug_build_local_only_and_token_gated";
}

inline auto debug_side_panel_policy() noexcept -> std::string_view {
    return "side_panel_is_a_client_of_the_same_debug_protocol_as_cli_agents";
}

inline auto debug_agent_control_policy() noexcept -> std::string_view {
    return "agents_use_semantic_targets_first_and_coordinates_when_needed";
}

inline auto input_frame_event_count(InputFrame const& frame) noexcept
        -> std::size_t {
    return frame.events.size();
}

inline auto input_script_event_count(InputScript const& script) noexcept
        -> std::size_t {
    std::size_t count = 0;
    for (auto const& frame : script.frames)
        count += frame.events.size();
    return count;
}

inline bool input_frame_is_replayable(InputFrame const& frame) {
    return frame.deterministic
        && std::ranges::all_of(frame.events, [](InputEvent const& event) {
            return event.sequence != 0
                || input_event_kind(event) == InputEventKind::Tick;
        });
}

inline bool input_script_is_replayable(InputScript const& script) {
    return script.deterministic
        && std::ranges::all_of(script.frames, input_frame_is_replayable);
}

inline bool output_observation_is_llm_debuggable(
        OutputObservation const& observation) {
    return observation.semantic_tree_present
        && observation.runtime_summary_present
        && observation.machine_readable_failure_shape
        && !observation.likely_layers.empty()
        && !observation.likely_passes.empty();
}

inline bool artifact_bundle_is_llm_debuggable(
        ArtifactBundleDescriptor const& bundle) {
    return bundle.snapshot_json
        && bundle.frame_image
        && bundle.platform_runtime_details
        && output_observation_is_llm_debuggable(bundle.observation);
}

inline bool debug_protocol_is_agent_complete() noexcept {
    return debug_protocol_domains.size() >= 9
        && debug_protocol_commands.size() >= 15
        && debug_panel_sections.size() >= 7
        && debug_transport_policy().find("json_commands")
            != std::string_view::npos
        && debug_security_policy().find("local_only")
            != std::string_view::npos
        && debug_agent_control_policy().find("semantic_targets")
            != std::string_view::npos;
}

} // namespace phenotype::io

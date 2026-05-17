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

} // namespace phenotype::io

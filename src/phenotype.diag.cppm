// phenotype.diag — OpenTelemetry-inspired logs + metrics for phenotype.
//
// Two namespaces under phenotype:::
//
//   phenotype::log     — structured log records with severity + scope.
//                        Default sink is stderr; the wasi-sdk fd_write
//                        polyfill in shim/phenotype.js routes that to
//                        console.error so no new host import is needed.
//
//   phenotype::metrics — Counter, Gauge, Histogram instruments registered
//                        statically. Names follow OTel conventions
//                        (`phenotype.runner.frame_duration` etc.).
//
// All instrument state is process-global and intended for the framework's
// own instrumentation sites — phenotype is single-threaded so the
// counters are plain `++`. JSON export goes through jsoncpp; future
// PRs can add a JS adapter that maps the exported shape to OTLP.

module;
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <format>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
export module phenotype.diag;

import json;
import phenotype.material;
import phenotype.types;

export namespace phenotype {

// =========================================================================
// log — structured logging
// =========================================================================
namespace log {

// OTel log severity numbers (https://opentelemetry.io/docs/specs/otel/logs/data-model)
enum class Severity : int {
    trace = 1,
    debug = 5,
    info  = 9,
    warn  = 13,
    error = 17,
    fatal = 21,
    off   = 100,  // synthetic — used as a filter threshold only
};

inline char const* severity_text(Severity s) noexcept {
    switch (s) {
        case Severity::trace: return "TRACE";
        case Severity::debug: return "DEBUG";
        case Severity::info:  return "INFO";
        case Severity::warn:  return "WARN";
        case Severity::error: return "ERROR";
        case Severity::fatal: return "FATAL";
        default:              return "OFF";
    }
}

// One log record. Stored in a fixed-size ring buffer for snapshot export.
struct Record {
    std::uint64_t time_unix_nano = 0;
    Severity      severity       = Severity::info;
    std::string   scope_name;     // e.g. "phenotype.runner"
    std::string   body;
};

namespace detail {
    inline Severity g_level = Severity::info;

    constexpr std::size_t LOG_RING_CAPACITY = 256;

    struct LogRing {
        std::array<Record, LOG_RING_CAPACITY> entries{};
        std::size_t                            head  = 0;  // next write position
        std::size_t                            count = 0;  // current size, capped
    };

    inline LogRing& ring() {
        // Heap-bound reference: the LogRing has dynamic-storage duration so
        // wasi-sdk's crt1-command.o never destroys it after _start() returns.
        // Same intent as the placement-new trick g_app uses in
        // phenotype_state.cppm; standard C++ so it stays portable to MSVC
        // when the Direct3D backend lands.
        static LogRing& r = *new LogRing();
        return r;
    }

    void emit_formatted(Severity sev, char const* scope, std::string body) noexcept;
} // namespace detail

inline Severity current_level() noexcept { return detail::g_level; }
inline void set_level(Severity s) noexcept { detail::g_level = s; }

// std::format-based convenience templates. Each level checks the
// runtime threshold *before* doing any formatting work, so disabled
// log calls are O(1).
template<typename... Args>
inline void trace(char const* scope, std::string_view fmt, Args&&... args) {
    if (Severity::trace < current_level()) return;
    detail::emit_formatted(Severity::trace, scope,
        std::vformat(fmt, std::make_format_args(args...)));
}
template<typename... Args>
inline void debug(char const* scope, std::string_view fmt, Args&&... args) {
    if (Severity::debug < current_level()) return;
    detail::emit_formatted(Severity::debug, scope,
        std::vformat(fmt, std::make_format_args(args...)));
}
template<typename... Args>
inline void info(char const* scope, std::string_view fmt, Args&&... args) {
    if (Severity::info < current_level()) return;
    detail::emit_formatted(Severity::info, scope,
        std::vformat(fmt, std::make_format_args(args...)));
}
template<typename... Args>
inline void warn(char const* scope, std::string_view fmt, Args&&... args) {
    if (Severity::warn < current_level()) return;
    detail::emit_formatted(Severity::warn, scope,
        std::vformat(fmt, std::make_format_args(args...)));
}
template<typename... Args>
inline void error(char const* scope, std::string_view fmt, Args&&... args) {
    if (Severity::error < current_level()) return;
    detail::emit_formatted(Severity::error, scope,
        std::vformat(fmt, std::make_format_args(args...)));
}

} // namespace log

// =========================================================================
// metrics — Counter / Gauge / Histogram instruments
// =========================================================================
namespace metrics {

struct Attribute {
    std::string key;
    std::string value;

    friend bool operator==(Attribute const& a, Attribute const& b) noexcept {
        return a.key == b.key && a.value == b.value;
    }
};

namespace detail {
    inline std::uint64_t now_ns() noexcept {
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
    }

    inline std::uint64_t start_time_ns() {
        static std::uint64_t t = now_ns();
        return t;
    }
} // namespace detail

// Counter — monotonically increasing sum, OTel "Sum" with is_monotonic=true.
class Counter {
public:
    Counter(char const* name, char const* description, char const* unit) noexcept;

    void add(std::uint64_t delta = 1, std::vector<Attribute> attrs = {}) noexcept;

    // Total across all attribute combinations. Convenience for callers
    // that don't care about per-attribute breakdowns.
    std::uint64_t total() const noexcept;

    char const* name() const noexcept        { return name_; }
    char const* description() const noexcept { return description_; }
    char const* unit() const noexcept        { return unit_; }

    struct DataPoint {
        std::vector<Attribute> attributes;
        std::uint64_t          value = 0;
    };
    std::vector<DataPoint> const& data_points() const noexcept { return points_; }

    void reset() noexcept;

private:
    char const*            name_;
    char const*            description_;
    char const*            unit_;
    std::vector<DataPoint> points_;
};

// Gauge — current value (or running max).
class Gauge {
public:
    Gauge(char const* name, char const* description, char const* unit) noexcept;

    void set(std::int64_t value, std::vector<Attribute> attrs = {}) noexcept;
    void update_max(std::int64_t value, std::vector<Attribute> attrs = {}) noexcept;

    char const* name() const noexcept        { return name_; }
    char const* description() const noexcept { return description_; }
    char const* unit() const noexcept        { return unit_; }

    struct DataPoint {
        std::vector<Attribute> attributes;
        std::int64_t           value = 0;
    };
    std::vector<DataPoint> const& data_points() const noexcept { return points_; }

    void reset() noexcept;

private:
    char const*            name_;
    char const*            description_;
    char const*            unit_;
    std::vector<DataPoint> points_;
};

// Histogram — explicit-bucket distribution. Bounds are the OTel default
// for http.server.request.duration scaled to nanoseconds, which gives a
// 5ms..10s spread suitable for full-frame timings. Phase-level
// histograms reuse the same bounds for cross-instrument comparison.
class Histogram {
public:
    static constexpr std::array<double, 14> bounds_ns = {
        5e6,    1e7,    2.5e7,  5e7,    7.5e7,
        1e8,    2.5e8,  5e8,    7.5e8,  1e9,
        2.5e9,  5e9,    7.5e9,  1e10
    };

    Histogram(char const* name, char const* description, char const* unit) noexcept;

    void record(std::uint64_t value, std::vector<Attribute> attrs = {}) noexcept;

    char const* name() const noexcept        { return name_; }
    char const* description() const noexcept { return description_; }
    char const* unit() const noexcept        { return unit_; }

    struct DataPoint {
        std::vector<Attribute>                  attributes;
        std::uint64_t                           count = 0;
        std::uint64_t                           sum   = 0;
        // bucket_counts has bounds_ns.size() + 1 entries — last is +Inf overflow.
        std::array<std::uint64_t, bounds_ns.size() + 1> buckets{};
    };
    std::vector<DataPoint> const& data_points() const noexcept { return points_; }

    void reset() noexcept;

private:
    char const*            name_;
    char const*            description_;
    char const*            unit_;
    std::vector<DataPoint> points_;
};

// -------------------------------------------------------------------------
// Static instrument registry — all known phenotype instruments live here
// so callers can refer to them as `metrics::inst::rebuilds.add()` etc.
//
// Every instrument is a static reference bound to a heap-allocated instance
// (`inline T& foo = *new T{...}`) for the same reason g_app uses
// placement-new in phenotype_state.cppm: wasi-sdk's crt1-command.o runs
// __cxa_finalize after _start() returns, which would otherwise destroy
// these vectors out from under any JS callback (click, key, hover, scroll)
// that fires after main() exits, leaving every post-main entry point
// reading from a destroyed std::vector and trapping with "memory access
// out of bounds". The reference variable itself is trivially destructible
// so it doesn't register with __cxa_atexit; the underlying T lives on the
// heap and is reclaimed wholesale when the wasm instance is torn down by
// the JS GC at page unload. The pattern is portable standard C++, so the
// same fix keeps working on MSVC when the Direct3D backend lands —
// clang-only attributes (`[[clang::no_destroy]]`) would silently
// re-introduce the bug there.
// -------------------------------------------------------------------------
namespace inst {

inline Counter& rebuilds = *new Counter{
    "phenotype.runner.rebuilds", "View rebuilds executed by run<>", "{rebuild}"};
inline Counter& alloc_nodes = *new Counter{
    "phenotype.arena.allocations", "Arena alloc_node calls", "{node}"};
inline Counter& arena_resets = *new Counter{
    "phenotype.arena.resets", "Arena reset calls", "{reset}"};
inline Gauge&   live_nodes = *new Gauge{
    "phenotype.arena.live_nodes", "Current arena node_count", "{node}"};
inline Gauge&   max_nodes_seen = *new Gauge{
    "phenotype.arena.max_nodes_seen", "Max node_count seen across epochs", "{node}"};

inline Counter& messages_posted = *new Counter{
    "phenotype.dispatch.messages_posted", "Messages posted by detail::post", "{msg}"};
inline Counter& messages_drained = *new Counter{
    "phenotype.dispatch.messages_drained", "Messages drained by the runner", "{msg}"};
inline Gauge&   max_queue_depth = *new Gauge{
    "phenotype.dispatch.max_queue_depth", "Max message queue depth seen", "{msg}"};

inline Counter& input_events = *new Counter{
    "phenotype.input.events",
    "Input events observed by core and native shells (attributes: event, source, detail, result, callback_id, role)",
    "{event}"};

inline Counter& flush_calls = *new Counter{
    "phenotype.host.flush_calls", "Host phenotype_flush() invocations", "{call}"};
inline Counter& frames_skipped = *new Counter{
    "phenotype.runner.frames_skipped",
    "Frames where paint produced an identical cmd buffer to the previous frame and the flush was skipped",
    "{frame}"};
inline Counter& measure_text_calls = *new Counter{
    "phenotype.host.measure_text_calls",
    "Host phenotype_measure_text() invocations that crossed the JS↔WASM trampoline (cache misses)",
    "{call}"};
inline Counter& measure_text_cache_hits = *new Counter{
    "phenotype.host.measure_text_cache_hits",
    "Cache hits served from the in-process measure_text cache",
    "{hit}"};

inline Counter& layout_nodes_skipped = *new Counter{
    "phenotype.runner.layout_nodes_skipped",
    "Nodes whose layout was copied from the previous frame by diff",
    "{node}"};
inline Counter& layout_nodes_computed = *new Counter{
    "phenotype.runner.layout_nodes_computed",
    "Nodes whose layout was computed fresh by layout_node",
    "{node}"};
inline Counter& paint_subtrees_blitted = *new Counter{
    "phenotype.runner.paint_subtrees_blitted",
    "Subtrees whose command bytes were memcpy'd from the previous frame's buffer instead of re-walked",
    "{subtree}"};
inline Counter& paint_bytes_blitted = *new Counter{
    "phenotype.runner.paint_bytes_blitted",
    "Command-buffer bytes copied from prev_cmd_buf by the subtree paint cache",
    "By"};
inline Counter& scissor_emitted = *new Counter{
    "phenotype.runner.scissor_emitted",
    "Scissor command pairs emitted around dirty-root subtrees (excludes resets)",
    "{scissor}"};
inline Counter& paint_buffer_flushes = *new Counter{
    "phenotype.paint.buffer_flushes",
    "Mid-paint command-buffer flushes — incremented when an emit_* would overflow the fixed buffer and forces an early flush. A non-zero value is a leading indicator, not a drop on its own.",
    "{flush}"};
inline Counter& paint_buffer_overflow = *new Counter{
    "phenotype.paint.buffer_overflow",
    "Paint emit_* writes that exceeded the buffer even after a mid-paint flush; the command was dropped (attribute: opcode).",
    "{overflow}"};
inline Counter& keyed_lists_matched = *new Counter{
    "phenotype.runner.keyed_lists_matched",
    "Parents whose children_keyed salvage pass ran (at least one old-keyed child was available for matching)",
    "{list}"};
inline Counter& keyed_children_matched = *new Counter{
    "phenotype.runner.keyed_children_matched",
    "Children that had their layout + paint state recovered by the keyed salvage pass after a structural diff miss",
    "{node}"};
inline Counter& native_text_cache_hits = *new Counter{
    "phenotype.native.text_cache_hits",
    "Native text atlas cache hits (attributes: platform)",
    "{hit}"};
inline Counter& native_text_cache_misses = *new Counter{
    "phenotype.native.text_cache_misses",
    "Native text atlas cache misses (attributes: platform)",
    "{miss}"};
inline Counter& native_texture_upload_bytes = *new Counter{
    "phenotype.native.texture_upload_bytes",
    "Native renderer texture upload bytes (attributes: platform)",
    "By"};
inline Counter& native_buffer_reallocations = *new Counter{
    "phenotype.native.buffer_reallocations",
    "Native renderer GPU buffer reallocations (attributes: platform, buffer)",
    "{realloc}"};

inline Histogram& frame_duration = *new Histogram{
    "phenotype.runner.frame_duration",
    "Total frame duration (drain + update + view + layout + paint + flush)",
    "ns"};
inline Histogram& phase_duration = *new Histogram{
    "phenotype.runner.phase_duration",
    "Per-phase duration inside one rebuild (attribute: phase)",
    "ns"};
inline Histogram& native_phase_duration = *new Histogram{
    "phenotype.native.phase_duration",
    "Per-phase duration inside a native renderer flush (attributes: platform, phase)",
    "ns"};

} // namespace inst

// Reset every registered instrument. Tests use this; runtime callers
// shouldn't need it.
void reset_all() noexcept;

} // namespace metrics

// =========================================================================
// JSON snapshot — emit logs + metrics in an OTel-shaped jsoncpp::Value.
// JS adapter (separate PR) can map this to OTLP/JSON, OTLP/HTTP, etc.
// =========================================================================
namespace diag {
    json::Value build_snapshot();
    std::string serialize_snapshot();
} // namespace diag

} // namespace phenotype

// =========================================================================
// Implementation
// =========================================================================

namespace phenotype::log::detail {

inline void push_to_ring(Record rec) noexcept {
    auto& r = ring();
    r.entries[r.head] = std::move(rec);
    r.head = (r.head + 1) % LOG_RING_CAPACITY;
    if (r.count < LOG_RING_CAPACITY) ++r.count;
}

inline void emit_formatted(Severity sev, char const* scope, std::string body) noexcept {
    // Snapshot record
    Record rec{
        ::phenotype::metrics::detail::now_ns(),
        sev,
        std::string{scope},
        body
    };
    push_to_ring(std::move(rec));

    // stderr sink — wasi fd_write polyfill routes to console.error in browser
    std::fprintf(stderr, "[%s %s] %s\n", severity_text(sev), scope, body.c_str());
}

} // namespace phenotype::log::detail

namespace phenotype::metrics {

// -------------------------------------------------------------------------
// Counter
// -------------------------------------------------------------------------
inline Counter::Counter(char const* name, char const* description, char const* unit) noexcept
    : name_{name}, description_{description}, unit_{unit} {}

inline void Counter::add(std::uint64_t delta, std::vector<Attribute> attrs) noexcept {
    for (auto& dp : points_) {
        if (dp.attributes == attrs) { dp.value += delta; return; }
    }
    points_.push_back({std::move(attrs), delta});
}

inline std::uint64_t Counter::total() const noexcept {
    std::uint64_t sum = 0;
    for (auto const& dp : points_) sum += dp.value;
    return sum;
}

inline void Counter::reset() noexcept { points_.clear(); }

// -------------------------------------------------------------------------
// Gauge
// -------------------------------------------------------------------------
inline Gauge::Gauge(char const* name, char const* description, char const* unit) noexcept
    : name_{name}, description_{description}, unit_{unit} {}

inline void Gauge::set(std::int64_t value, std::vector<Attribute> attrs) noexcept {
    for (auto& dp : points_) {
        if (dp.attributes == attrs) { dp.value = value; return; }
    }
    points_.push_back({std::move(attrs), value});
}

inline void Gauge::update_max(std::int64_t value, std::vector<Attribute> attrs) noexcept {
    for (auto& dp : points_) {
        if (dp.attributes == attrs) {
            if (value > dp.value) dp.value = value;
            return;
        }
    }
    points_.push_back({std::move(attrs), value});
}

inline void Gauge::reset() noexcept { points_.clear(); }

// -------------------------------------------------------------------------
// Histogram
// -------------------------------------------------------------------------
inline Histogram::Histogram(char const* name, char const* description, char const* unit) noexcept
    : name_{name}, description_{description}, unit_{unit} {}

inline void Histogram::record(std::uint64_t value, std::vector<Attribute> attrs) noexcept {
    DataPoint* target = nullptr;
    for (auto& dp : points_) {
        if (dp.attributes == attrs) { target = &dp; break; }
    }
    if (!target) {
        points_.push_back({std::move(attrs), 0, 0, {}});
        target = &points_.back();
    }
    target->count += 1;
    target->sum   += value;
    auto v = static_cast<double>(value);
    for (std::size_t i = 0; i < bounds_ns.size(); ++i) {
        if (v <= bounds_ns[i]) { target->buckets[i] += 1; return; }
    }
    target->buckets[bounds_ns.size()] += 1;
}

inline void Histogram::reset() noexcept { points_.clear(); }

inline void reset_all() noexcept {
    inst::rebuilds.reset();
    inst::alloc_nodes.reset();
    inst::arena_resets.reset();
    inst::live_nodes.reset();
    inst::max_nodes_seen.reset();
    inst::messages_posted.reset();
    inst::messages_drained.reset();
    inst::max_queue_depth.reset();
    inst::input_events.reset();
    inst::flush_calls.reset();
    inst::frames_skipped.reset();
    inst::measure_text_calls.reset();
    inst::measure_text_cache_hits.reset();
    inst::layout_nodes_skipped.reset();
    inst::layout_nodes_computed.reset();
    inst::paint_subtrees_blitted.reset();
    inst::paint_bytes_blitted.reset();
    inst::scissor_emitted.reset();
    inst::paint_buffer_flushes.reset();
    inst::paint_buffer_overflow.reset();
    inst::keyed_lists_matched.reset();
    inst::keyed_children_matched.reset();
    inst::native_text_cache_hits.reset();
    inst::native_text_cache_misses.reset();
    inst::native_texture_upload_bytes.reset();
    inst::native_buffer_reallocations.reset();
    inst::frame_duration.reset();
    inst::phase_duration.reset();
    inst::native_phase_duration.reset();
    // NOTE: g_app.last_paint_hash is NOT reset here — phenotype.diag
    // cannot import phenotype.state (cycle: state already imports diag).
    // Tests that need a clean hash state assign detail::g_app.last_paint_hash
    // = 0 directly themselves.
    auto& ring = ::phenotype::log::detail::ring();
    ring.head = 0;
    ring.count = 0;
}

} // namespace phenotype::metrics

export namespace phenotype::diag {

struct RectSnapshot {
    bool valid = false;
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
};

struct InputDebugSnapshot {
    std::string event;
    std::string source;
    std::string detail;
    std::string result;
    unsigned int callback_id = 0xFFFFFFFFu;
    std::string role = "none";
    unsigned int focused_id = 0xFFFFFFFFu;
    std::string focused_role = "none";
    bool focus_visible = false;
    std::string input_modality = "none";
    std::string focus_visibility_reason = "no_focus";
    unsigned int hovered_id = 0xFFFFFFFFu;
    unsigned int pressed_id = 0xFFFFFFFFu;
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    unsigned int caret_pos = 0xFFFFFFFFu;
    bool selection_active = false;
    unsigned int selection_start = 0xFFFFFFFFu;
    unsigned int selection_end = 0xFFFFFFFFu;
    bool caret_visible = true;
    std::string caret_renderer = "hidden";
    RectSnapshot caret_rect{};
    RectSnapshot caret_draw_rect{};
    RectSnapshot caret_host_rect{};
    RectSnapshot caret_screen_rect{};
    RectSnapshot caret_host_bounds{};
    bool caret_host_flipped = false;
    std::string caret_geometry_source = "draw";
    bool focused_is_input = false;
    bool composition_active = false;
    std::string composition_text;
    unsigned int composition_cursor = 0;
};

struct SemanticNodeSnapshot {
    std::optional<unsigned int> callback_id;
    std::string role = "none";
    std::string label;
    struct MaterialSnapshot {
        std::string kind;
        std::string role;
        float opacity = 0.0f;
        float blur_radius = 0.0f;
        Color tint = {0, 0, 0, 0};
        float saturation = 1.0f;
        float luminance_floor = 0.0f;
        float luminance_gain = 1.0f;
        float edge_highlight = 0.0f;
        float edge_width = 1.0f;
        float noise_opacity = 0.0f;
        float shadow_alpha = 0.0f;
        float shadow_radius = 0.0f;
        MaterialContainerDescriptor container{};
        MaterialProminenceDescriptor prominence{};
        bool fallback = false;
        std::string fallback_reason;
        std::string contrast_intent;
        std::string plan_id;
        std::string verifier_profile;
    };
    std::optional<MaterialSnapshot> material;
    RectSnapshot bounds{};
    bool visible = false;
    bool enabled = true;
    bool focusable = false;
    bool focused = false;
    bool scroll_container = false;
    std::vector<SemanticNodeSnapshot> children;
};

struct PlatformCapabilitiesSnapshot {
    std::string platform;
    bool read_only = true;
    bool snapshot_json = true;
    bool capture_frame_rgba = false;
    bool write_artifact_bundle = false;
    bool semantic_tree = true;
    bool input_debug = true;
    bool platform_runtime = true;
    bool frame_image = false;
    bool platform_diagnostics = false;
    bool material_surfaces = true;
    bool material_backdrop_blur = false;
    bool reduce_transparency = false;
    bool increase_contrast = false;
    bool reduce_motion = false;
    bool material_shader_blur = false;
    bool material_frame_history = false;
    unsigned int material_max_shader_sample_taps = 0;
    std::int64_t material_max_texture_dimension_2d = 0;
    std::int64_t material_max_backdrop_pixels = 0;
    std::string material_capability_profile = "generic";
    std::string material_capability_source = "default";
    PlatformSystemSettingsSnapshot system_settings{};
};

struct PlatformRuntimeSnapshot {
    std::string platform;
    std::string backend;
    RectSnapshot viewport{};
    float scroll_x = 0.0f;
    float scroll_y = 0.0f;
    float content_height = 0.0f;
    std::optional<unsigned int> focused_callback_id;
    bool focus_visible = false;
    std::string input_modality = "none";
    std::string focus_visibility_reason = "no_focus";
    std::optional<unsigned int> hovered_callback_id;
    std::optional<unsigned int> pressed_callback_id;
    json::Value details = json::Value{json::Object{}};
};

struct DebugPlaneSnapshot {
    PlatformCapabilitiesSnapshot platform_capabilities{};
    InputDebugSnapshot input_debug{};
    std::optional<SemanticNodeSnapshot> semantic_tree;
    PlatformRuntimeSnapshot platform_runtime{};
};

namespace detail {
    struct ArtifactFrameCapture {
        unsigned int width = 0;
        unsigned int height = 0;
        std::vector<std::uint8_t> rgba;
    };

    struct ArtifactBundleResult {
        bool ok = false;
        std::string directory;
        std::string snapshot_json_path;
        std::string frame_image_path;
        std::vector<std::string> platform_files;
        std::string error;
    };

    using DebugPayloadBuilder = json::Value (*)();
    using ApplicationDebugProvider = json::Value (*)();
    using PlatformCapabilitiesProvider = PlatformCapabilitiesSnapshot (*)();
    using PlatformRuntimeDetailsProvider = json::Value (*)();

    DebugPayloadBuilder& debug_payload_builder_storage() noexcept {
        static DebugPayloadBuilder builder = nullptr;
        return builder;
    }

    ApplicationDebugProvider& application_debug_provider_storage() noexcept {
        static ApplicationDebugProvider provider = nullptr;
        return provider;
    }

    PlatformCapabilitiesProvider& platform_capabilities_provider_storage() noexcept {
        static PlatformCapabilitiesProvider provider = nullptr;
        return provider;
    }

    PlatformRuntimeDetailsProvider& platform_runtime_details_provider_storage() noexcept {
        static PlatformRuntimeDetailsProvider provider = nullptr;
        return provider;
    }

    void set_debug_payload_builder(DebugPayloadBuilder builder) noexcept {
        debug_payload_builder_storage() = builder;
    }

    void set_application_debug_provider(ApplicationDebugProvider provider) noexcept {
        application_debug_provider_storage() = provider;
    }

    void set_platform_capabilities_provider(PlatformCapabilitiesProvider provider) noexcept {
        platform_capabilities_provider_storage() = provider;
    }

    void set_platform_runtime_details_provider(PlatformRuntimeDetailsProvider provider) noexcept {
        platform_runtime_details_provider_storage() = provider;
    }

    DebugPayloadBuilder current_debug_payload_builder() noexcept {
        return debug_payload_builder_storage();
    }

    ApplicationDebugProvider current_application_debug_provider() noexcept {
        return application_debug_provider_storage();
    }

    PlatformCapabilitiesSnapshot current_platform_capabilities() {
        if (auto provider = platform_capabilities_provider_storage())
            return provider();
        return {};
    }

    json::Value current_platform_runtime_details() {
        if (auto provider = platform_runtime_details_provider_storage())
            return provider();
        return json::Value{json::Object{}};
    }

    inline json::Value material_container_descriptor_json(
            MaterialContainerDescriptor const& descriptor) {
        json::Object out;
        out.emplace(
            "mode",
            json::Value{material_container_mode_name(descriptor.mode())});
        out.emplace(
            "container_id",
            json::Value{
                static_cast<std::int64_t>(descriptor.container_id)});
        out.emplace(
            "union_id",
            json::Value{static_cast<std::int64_t>(descriptor.union_id)});
        out.emplace("spacing", json::Value{descriptor.spacing});
        out.emplace("interactive", json::Value{descriptor.interactive});
        out.emplace(
            "morph_transitions",
            json::Value{descriptor.morph_transitions});
        return json::Value{std::move(out)};
    }

    inline json::Value material_container_analysis_json(
            MaterialContainerAnalysis const& analysis) {
        json::Object out;
        out.emplace("mode", json::Value{analysis.mode_name});
        out.emplace(
            "container_id",
            json::Value{
                static_cast<std::int64_t>(analysis.container_id)});
        out.emplace(
            "union_id",
            json::Value{static_cast<std::int64_t>(analysis.union_id)});
        out.emplace("spacing", json::Value{analysis.spacing});
        out.emplace("blend_distance", json::Value{analysis.blend_distance});
        out.emplace("interactive", json::Value{analysis.interactive});
        out.emplace(
            "requested_morph_transitions",
            json::Value{analysis.requested_morph_transitions});
        out.emplace(
            "morph_transitions",
            json::Value{analysis.morph_transitions});
        out.emplace("participates", json::Value{analysis.participates});
        out.emplace(
            "shared_backdrop_scope",
            json::Value{analysis.shared_backdrop_scope});
        out.emplace(
            "shape_union_expected",
            json::Value{analysis.shape_union_expected});
        out.emplace(
            "shape_blending_expected",
            json::Value{analysis.shape_blending_expected});
        out.emplace(
            "reduced_motion_suppressed_morph",
            json::Value{analysis.reduced_motion_suppressed_morph});
        out.emplace("spacing_clamped", json::Value{analysis.spacing_clamped});
        out.emplace("blend_policy", json::Value{analysis.blend_policy});
        out.emplace("morph_policy", json::Value{analysis.morph_policy});
        out.emplace(
            "performance_policy",
            json::Value{analysis.performance_policy});
        return json::Value{std::move(out)};
    }

    inline json::Value material_interaction_descriptor_json(
            MaterialInteractionDescriptor const& descriptor) {
        json::Object out;
        out.emplace("hovered", json::Value{descriptor.hovered});
        out.emplace("pressed", json::Value{descriptor.pressed});
        out.emplace("focused", json::Value{descriptor.focused});
        out.emplace("pointer_inside", json::Value{descriptor.pointer_inside});
        out.emplace("pointer_x", json::Value{descriptor.pointer_x});
        out.emplace("pointer_y", json::Value{descriptor.pointer_y});
        out.emplace("active", json::Value{descriptor.active()});
        return json::Value{std::move(out)};
    }

    inline json::Value material_transition_descriptor_json(
            MaterialTransitionDescriptor const& descriptor) {
        json::Object out;
        out.emplace(
            "kind",
            json::Value{
                material_glass_transition_kind_name(descriptor.kind)});
        out.emplace("progress", json::Value{descriptor.progress});
        out.emplace("appearing", json::Value{descriptor.appearing});
        return json::Value{std::move(out)};
    }

    inline json::Value material_glass_identity_descriptor_json(
            MaterialGlassIdentityDescriptor const& descriptor) {
        json::Object out;
        out.emplace(
            "namespace_id",
            json::Value{static_cast<std::int64_t>(descriptor.namespace_id)});
        out.emplace(
            "effect_id",
            json::Value{static_cast<std::int64_t>(descriptor.effect_id)});
        out.emplace("participates", json::Value{descriptor.participates()});
        return json::Value{std::move(out)};
    }

    inline json::Value material_transition_analysis_json(
            MaterialTransitionAnalysis const& analysis) {
        json::Object out;
        out.emplace("kind", json::Value{analysis.kind_name});
        out.emplace("active", json::Value{analysis.active});
        out.emplace("materialize", json::Value{analysis.materialize});
        out.emplace("matched_geometry", json::Value{analysis.matched_geometry});
        out.emplace("appearing", json::Value{analysis.appearing});
        out.emplace(
            "reduced_motion_suppressed",
            json::Value{analysis.reduced_motion_suppressed});
        out.emplace("bounded", json::Value{analysis.bounded});
        out.emplace("progress", json::Value{analysis.progress});
        out.emplace("opacity_gain", json::Value{analysis.opacity_gain});
        out.emplace("optical_gain", json::Value{analysis.optical_gain});
        out.emplace("shadow_gain", json::Value{analysis.shadow_gain});
        out.emplace(
            "refraction_gain",
            json::Value{analysis.refraction_gain});
        out.emplace(
            "materialize_optics_model",
            json::Value{analysis.materialize_optics_model});
        out.emplace(
            "materialize_optics_active",
            json::Value{analysis.materialize_optics_active});
        out.emplace(
            "materialize_wave_strength",
            json::Value{analysis.materialize_wave_strength});
        out.emplace(
            "materialize_edge_lift",
            json::Value{analysis.materialize_edge_lift});
        out.emplace(
            "materialize_lensing_gain",
            json::Value{analysis.materialize_lensing_gain});
        out.emplace(
            "materialize_rim_position",
            json::Value{analysis.materialize_rim_position});
        out.emplace("policy", json::Value{analysis.policy});
        return json::Value{std::move(out)};
    }

    inline json::Value material_glass_identity_analysis_json(
            MaterialGlassIdentityAnalysis const& analysis) {
        json::Object out;
        out.emplace(
            "namespace_id",
            json::Value{static_cast<std::int64_t>(analysis.namespace_id)});
        out.emplace(
            "effect_id",
            json::Value{static_cast<std::int64_t>(analysis.effect_id)});
        out.emplace("namespace_scoped", json::Value{analysis.namespace_scoped});
        out.emplace("effect_identified", json::Value{analysis.effect_identified});
        out.emplace("participates", json::Value{analysis.participates});
        out.emplace("container_scoped", json::Value{analysis.container_scoped});
        out.emplace(
            "matched_geometry_candidate",
            json::Value{analysis.matched_geometry_candidate});
        out.emplace("bounded", json::Value{analysis.bounded});
        out.emplace("source", json::Value{analysis.source});
        out.emplace("policy", json::Value{analysis.policy});
        return json::Value{std::move(out)};
    }

    inline json::Value color_to_json(Color color) {
        json::Object out;
        out.emplace(
            "r",
            json::Value{static_cast<std::int64_t>(color.r)});
        out.emplace(
            "g",
            json::Value{static_cast<std::int64_t>(color.g)});
        out.emplace(
            "b",
            json::Value{static_cast<std::int64_t>(color.b)});
        out.emplace(
            "a",
            json::Value{static_cast<std::int64_t>(color.a)});
        return json::Value{std::move(out)};
    }

    inline json::Value material_scroll_edge_profile_json(
            MaterialScrollEdgeProfile const& profile) {
        json::Object out;
        out.emplace("model", json::Value{profile.model});
        out.emplace("source", json::Value{profile.source});
        out.emplace("active", json::Value{profile.active});
        out.emplace("role_driven", json::Value{profile.role_driven});
        out.emplace("backdrop_driven", json::Value{profile.backdrop_driven});
        out.emplace("contrast_driven", json::Value{profile.contrast_driven});
        out.emplace("hard_style", json::Value{profile.hard_style});
        out.emplace("bounded", json::Value{profile.bounded});
        out.emplace(
            "fade_extent_pixels",
            json::Value{profile.fade_extent_pixels});
        out.emplace(
            "dissolve_strength",
            json::Value{profile.dissolve_strength});
        out.emplace(
            "dimming_strength",
            json::Value{profile.dimming_strength});
        out.emplace(
            "hard_style_strength",
            json::Value{profile.hard_style_strength});
        return json::Value{std::move(out)};
    }

    inline json::Value material_clear_glass_legibility_profile_json(
            MaterialClearGlassLegibilityProfile const& profile) {
        json::Object out;
        out.emplace("model", json::Value{profile.model});
        out.emplace("source", json::Value{profile.source});
        out.emplace("active", json::Value{profile.active});
        out.emplace("backdrop_driven", json::Value{profile.backdrop_driven});
        out.emplace(
            "brightness_driven",
            json::Value{profile.brightness_driven});
        out.emplace("detail_driven", json::Value{profile.detail_driven});
        out.emplace("contrast_driven", json::Value{profile.contrast_driven});
        out.emplace(
            "accessibility_driven",
            json::Value{profile.accessibility_driven});
        out.emplace("bounded", json::Value{profile.bounded});
        out.emplace(
            "dimming_strength",
            json::Value{profile.dimming_strength});
        out.emplace("contrast_lift", json::Value{profile.contrast_lift});
        out.emplace(
            "brightness_response",
            json::Value{profile.brightness_response});
        out.emplace(
            "detail_response",
            json::Value{profile.detail_response});
        return json::Value{std::move(out)};
    }

    inline json::Value material_plan_runtime_json(
            MaterialRuntimeRecord const& record) {
        auto const& plan = record.plan;
        json::Object geometry;
        geometry.emplace("x", json::Value{plan.geometry.x});
        geometry.emplace("y", json::Value{plan.geometry.y});
        geometry.emplace("w", json::Value{plan.geometry.w});
        geometry.emplace("h", json::Value{plan.geometry.h});
        geometry.emplace("radius", json::Value{plan.geometry.radius});

        json::Object shape;
        shape.emplace("valid", json::Value{plan.shape.valid});
        shape.emplace(
            "kind",
            json::Value{material_shape_kind_name(plan.shape.kind)});
        shape.emplace("rounded", json::Value{plan.shape.rounded});
        shape.emplace("capsule", json::Value{plan.shape.capsule});
        shape.emplace("radius_clamped",
                      json::Value{plan.shape.radius_clamped});
        shape.emplace("surface_area",
                      json::Value{plan.shape.surface_area});
        shape.emplace("min_extent", json::Value{plan.shape.min_extent});
        shape.emplace("max_extent", json::Value{plan.shape.max_extent});
        shape.emplace("radius_limit",
                      json::Value{plan.shape.radius_limit});
        shape.emplace("effective_radius",
                      json::Value{plan.shape.effective_radius});
        shape.emplace("normalized_radius",
                      json::Value{plan.shape.normalized_radius});

        json::Object render_target;
        render_target.emplace(
            "width",
            json::Value{static_cast<std::int64_t>(plan.render_target.width)});
        render_target.emplace(
            "height",
            json::Value{static_cast<std::int64_t>(plan.render_target.height)});
        render_target.emplace("scale", json::Value{plan.render_target.scale});
        render_target.emplace(
            "pixel_format",
            json::Value{plan.render_target.pixel_format});
        render_target.emplace(
            "pixel_count",
            json::Value{plan.render_target.pixel_count});
        render_target.emplace("ready", json::Value{plan.render_target.ready});
        render_target.emplace(
            "within_backdrop_budget",
            json::Value{plan.render_target.within_backdrop_budget});

        json::Object capability_snapshot;
        capability_snapshot.emplace(
            "material_surfaces",
            json::Value{plan.capability_snapshot.material_surfaces});
        capability_snapshot.emplace(
            "material_backdrop_blur",
            json::Value{plan.capability_snapshot.material_backdrop_blur});
        capability_snapshot.emplace(
            "shader_blur",
            json::Value{plan.capability_snapshot.shader_blur});
        capability_snapshot.emplace(
            "frame_history",
            json::Value{plan.capability_snapshot.frame_history});
        capability_snapshot.emplace(
            "reduce_transparency",
            json::Value{plan.capability_snapshot.reduce_transparency});
        capability_snapshot.emplace(
            "increase_contrast",
            json::Value{plan.capability_snapshot.increase_contrast});
        capability_snapshot.emplace(
            "reduce_motion",
            json::Value{plan.capability_snapshot.reduce_motion});
        capability_snapshot.emplace(
            "max_shader_sample_taps",
            json::Value{static_cast<std::int64_t>(
                plan.capability_snapshot.max_shader_sample_taps)});
        capability_snapshot.emplace(
            "max_texture_dimension_2d",
            json::Value{plan.capability_snapshot.max_texture_dimension_2d});
        capability_snapshot.emplace(
            "max_backdrop_pixels",
            json::Value{plan.capability_snapshot.max_backdrop_pixels});
        capability_snapshot.emplace(
            "texture_limits_known",
            json::Value{plan.capability_snapshot.texture_limits_known});
        capability_snapshot.emplace(
            "backdrop_budget_known",
            json::Value{plan.capability_snapshot.backdrop_budget_known});
        capability_snapshot.emplace(
            "target_within_texture_limits",
            json::Value{
                plan.capability_snapshot.target_within_texture_limits});
        capability_snapshot.emplace(
            "target_within_backdrop_budget",
            json::Value{
                plan.capability_snapshot.target_within_backdrop_budget});
        capability_snapshot.emplace(
            "profile",
            json::Value{plan.capability_snapshot.profile});
        capability_snapshot.emplace(
            "source",
            json::Value{plan.capability_snapshot.source});

        json::Object decision_trace;
        decision_trace.emplace(
            "has_geometry",
            json::Value{plan.decision_trace.has_geometry});
        decision_trace.emplace(
            "has_material",
            json::Value{plan.decision_trace.has_material});
        decision_trace.emplace(
            "role_allows_liquid_glass",
            json::Value{plan.decision_trace.role_allows_liquid_glass});
        decision_trace.emplace(
            "content_layer_standard_material",
            json::Value{
                plan.decision_trace.content_layer_standard_material});
        decision_trace.emplace(
            "liquid_glass_backdrop_candidate",
            json::Value{
                plan.decision_trace.liquid_glass_backdrop_candidate});
        decision_trace.emplace(
            "target_ready",
            json::Value{plan.decision_trace.target_ready});
        decision_trace.emplace(
            "quality_switches_allow_backdrop",
            json::Value{
                plan.decision_trace.quality_switches_allow_backdrop});
        decision_trace.emplace(
            "backdrop_pixels_within_budget",
            json::Value{
                plan.decision_trace.backdrop_pixels_within_budget});
        decision_trace.emplace(
            "quality_allows_backdrop",
            json::Value{plan.decision_trace.quality_allows_backdrop});
        decision_trace.emplace(
            "capability_material_surfaces",
            json::Value{plan.decision_trace.capability_material_surfaces});
        decision_trace.emplace(
            "capability_material_backdrop_blur",
            json::Value{
                plan.decision_trace.capability_material_backdrop_blur});
        decision_trace.emplace(
            "capability_shader_blur",
            json::Value{plan.decision_trace.capability_shader_blur});
        decision_trace.emplace(
            "capability_frame_history",
            json::Value{plan.decision_trace.capability_frame_history});
        decision_trace.emplace(
            "capability_texture_limits_known",
            json::Value{
                plan.decision_trace.capability_texture_limits_known});
        decision_trace.emplace(
            "capability_backdrop_budget_known",
            json::Value{
                plan.decision_trace.capability_backdrop_budget_known});
        decision_trace.emplace(
            "capability_target_within_texture_limits",
            json::Value{
                plan.decision_trace.capability_target_within_texture_limits});
        decision_trace.emplace(
            "capability_target_within_backdrop_budget",
            json::Value{
                plan.decision_trace.capability_target_within_backdrop_budget});
        decision_trace.emplace(
            "backend_supports_backdrop",
            json::Value{plan.decision_trace.backend_supports_backdrop});
        decision_trace.emplace(
            "backdrop_available",
            json::Value{plan.decision_trace.backdrop_available});
        decision_trace.emplace(
            "backdrop_stable",
            json::Value{plan.decision_trace.backdrop_stable});
        decision_trace.emplace(
            "backdrop_source_ready",
            json::Value{plan.decision_trace.backdrop_source_ready});
        decision_trace.emplace(
            "next_frame_capture_required",
            json::Value{
                plan.decision_trace.next_frame_capture_required});
        decision_trace.emplace(
            "reduced_transparency",
            json::Value{plan.decision_trace.reduced_transparency});
        decision_trace.emplace(
            "increase_contrast",
            json::Value{plan.decision_trace.increase_contrast});
        decision_trace.emplace(
            "reduce_motion",
            json::Value{plan.decision_trace.reduce_motion});
        decision_trace.emplace(
            "can_sample_backdrop",
            json::Value{plan.decision_trace.can_sample_backdrop});
        decision_trace.emplace(
            "first_blocker",
            json::Value{plan.decision_trace.first_blocker});

        json::Object tint;
        tint.emplace("r", json::Value{static_cast<std::int64_t>(plan.tint.r)});
        tint.emplace("g", json::Value{static_cast<std::int64_t>(plan.tint.g)});
        tint.emplace("b", json::Value{static_cast<std::int64_t>(plan.tint.b)});
        tint.emplace("a", json::Value{static_cast<std::int64_t>(plan.tint.a)});

        json::Object descriptor_tint;
        descriptor_tint.emplace(
            "r",
            json::Value{
                static_cast<std::int64_t>(plan.command_descriptor.tint.r)});
        descriptor_tint.emplace(
            "g",
            json::Value{
                static_cast<std::int64_t>(plan.command_descriptor.tint.g)});
        descriptor_tint.emplace(
            "b",
            json::Value{
                static_cast<std::int64_t>(plan.command_descriptor.tint.b)});
        descriptor_tint.emplace(
            "a",
            json::Value{
                static_cast<std::int64_t>(plan.command_descriptor.tint.a)});

        json::Object command_descriptor;
        command_descriptor.emplace(
            "kind",
            json::Value{material_kind_name(plan.command_descriptor.kind)});
        command_descriptor.emplace(
            "role",
            json::Value{
                material_surface_role_name(plan.command_descriptor.role)});
        command_descriptor.emplace(
            "container",
            material_container_descriptor_json(
                plan.command_descriptor.container));
        command_descriptor.emplace(
            "interaction",
            material_interaction_descriptor_json(
                plan.command_descriptor.interaction));
        command_descriptor.emplace(
            "transition",
            material_transition_descriptor_json(
                plan.command_descriptor.transition));
        command_descriptor.emplace(
            "glass_identity",
            material_glass_identity_descriptor_json(
                plan.command_descriptor.glass_identity));
        command_descriptor.emplace(
            "opacity",
            json::Value{plan.command_descriptor.opacity});
        command_descriptor.emplace(
            "blur_radius",
            json::Value{plan.command_descriptor.blur_radius});
        command_descriptor.emplace(
            "tint",
            json::Value{std::move(descriptor_tint)});
        command_descriptor.emplace(
            "saturation",
            json::Value{plan.command_descriptor.saturation});
        command_descriptor.emplace(
            "luminance_floor",
            json::Value{plan.command_descriptor.luminance_floor});
        command_descriptor.emplace(
            "luminance_gain",
            json::Value{plan.command_descriptor.luminance_gain});
        command_descriptor.emplace(
            "edge_highlight",
            json::Value{plan.command_descriptor.edge_highlight});
        command_descriptor.emplace(
            "edge_width",
            json::Value{plan.command_descriptor.edge_width});
        command_descriptor.emplace(
            "noise_opacity",
            json::Value{plan.command_descriptor.noise_opacity});
        command_descriptor.emplace(
            "shadow_alpha",
            json::Value{plan.command_descriptor.shadow_alpha});
        command_descriptor.emplace(
            "shadow_radius",
            json::Value{plan.command_descriptor.shadow_radius});
        json::Object descriptor_prominence;
        descriptor_prominence.emplace(
            "enabled",
            json::Value{plan.command_descriptor.prominence.enabled});
        descriptor_prominence.emplace(
            "intensity",
            json::Value{plan.command_descriptor.prominence.intensity});
        command_descriptor.emplace(
            "prominence",
            json::Value{std::move(descriptor_prominence)});

        json::Object reference_model;
        reference_model.emplace(
            "technology",
            json::Value{plan.reference_model.technology});
        reference_model.emplace(
            "layer",
            json::Value{plan.reference_model.layer});
        reference_model.emplace(
            "material_policy",
            json::Value{plan.reference_model.material_policy});
        reference_model.emplace(
            "variant",
            json::Value{plan.reference_model.variant});
        reference_model.emplace(
            "shape",
            json::Value{plan.reference_model.shape});
        reference_model.emplace(
            "shape_scope",
            json::Value{plan.reference_model.shape_scope});
        reference_model.emplace(
            "blending_scope",
            json::Value{plan.reference_model.blending_scope});
        reference_model.emplace(
            "semantic_thickness",
            json::Value{plan.reference_model.semantic_thickness});
        reference_model.emplace(
            "accessibility_response",
            json::Value{plan.reference_model.accessibility_response});
        reference_model.emplace(
            "performance_response",
            json::Value{plan.reference_model.performance_response});
        reference_model.emplace(
            "view_bounds_anchored",
            json::Value{plan.reference_model.view_bounds_anchored});
        reference_model.emplace(
            "shape_matches_geometry",
            json::Value{plan.reference_model.shape_matches_geometry});
        reference_model.emplace(
            "tint_applied",
            json::Value{plan.reference_model.tint_applied});
        reference_model.emplace(
            "interactive_response",
            json::Value{plan.reference_model.interactive_response});
        reference_model.emplace(
            "container_grouped",
            json::Value{plan.reference_model.container_grouped});
        reference_model.emplace(
            "container_union",
            json::Value{plan.reference_model.container_union});
        reference_model.emplace(
            "container_morphing",
            json::Value{plan.reference_model.container_morphing});
        reference_model.emplace(
            "glass_effect_identified",
            json::Value{plan.reference_model.glass_effect_identified});
        reference_model.emplace(
            "glass_effect_matched_geometry",
            json::Value{
                plan.reference_model.glass_effect_matched_geometry});
        reference_model.emplace(
            "glass_background_effect",
            json::Value{plan.reference_model.glass_background_effect});
        reference_model.emplace(
            "glass_background_plate",
            json::Value{plan.reference_model.glass_background_plate});
        reference_model.emplace(
            "glass_background_feathered",
            json::Value{plan.reference_model.glass_background_feathered});
        reference_model.emplace(
            "legibility_preserved",
            json::Value{plan.reference_model.legibility_preserved});
        reference_model.emplace(
            "vibrancy_expected",
            json::Value{plan.reference_model.vibrancy_expected});
        reference_model.emplace(
            "deterministic_degradation",
            json::Value{plan.reference_model.deterministic_degradation});

        json::Object verifier;
        verifier.emplace(
            "require_backdrop_source",
            json::Value{plan.verifier.require_backdrop_source});
        verifier.emplace(
            "require_edge_highlight",
            json::Value{plan.verifier.require_edge_highlight});
        verifier.emplace(
            "require_container_identity",
            json::Value{plan.verifier.require_container_identity});
        verifier.emplace(
            "require_container_morph_contract",
            json::Value{plan.verifier.require_container_morph_contract});
        verifier.emplace(
            "min_luma_delta",
            json::Value{plan.verifier.min_luma_delta});
        verifier.emplace(
            "min_unique_colors",
            json::Value{
                static_cast<std::int64_t>(plan.verifier.min_unique_colors)});
        verifier.emplace("region_name", json::Value{plan.verifier.region_name});
        verifier.emplace("likely_layer", json::Value{plan.verifier.likely_layer});
        verifier.emplace("likely_pass", json::Value{plan.verifier.likely_pass});

        auto const& observation = plan.observation_contract;
        json::Object observation_contract;
        observation_contract.emplace(
            "schema_version",
            json::Value{
                static_cast<std::int64_t>(observation.schema_version)});
        observation_contract.emplace(
            "semantic_material_required",
            json::Value{observation.semantic_material_required});
        observation_contract.emplace(
            "runtime_plan_required",
            json::Value{observation.runtime_plan_required});
        observation_contract.emplace(
            "fallback_expected",
            json::Value{observation.fallback_expected});
        observation_contract.emplace(
            "backdrop_sampling_expected",
            json::Value{observation.backdrop_sampling_expected});
        observation_contract.emplace(
            "stable_backdrop_required",
            json::Value{observation.stable_backdrop_required});
        observation_contract.emplace(
            "shared_frame_capture_required",
            json::Value{observation.shared_frame_capture_required});
        observation_contract.emplace(
            "next_frame_capture_required",
            json::Value{observation.next_frame_capture_required});
        observation_contract.emplace(
            "backdrop_capture_excludes_foreground_text",
            json::Value{
                observation.backdrop_capture_excludes_foreground_text});
        observation_contract.emplace(
            "bounded_texture_copy_required",
            json::Value{observation.bounded_texture_copy_required});
        observation_contract.emplace(
            "deterministic_fallback_required",
            json::Value{observation.deterministic_fallback_required});
        observation_contract.emplace(
            "backdrop_capture_scope",
            json::Value{observation.backdrop_capture_scope});
        observation_contract.emplace(
            "backdrop_capture_reason",
            json::Value{observation.backdrop_capture_reason});
        observation_contract.emplace(
            "fallback_path",
            json::Value{observation.fallback_path});
        observation_contract.emplace(
            "fallback_reason",
            json::Value{observation.fallback_reason});
        observation_contract.emplace(
            "primary_pass",
            json::Value{observation.primary_pass});
        observation_contract.emplace(
            "primary_executor",
            json::Value{observation.primary_executor});
        observation_contract.emplace(
            "expected_stage_order",
            json::Value{observation.expected_stage_order});
        observation_contract.emplace(
            "expected_runtime_passes",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_runtime_passes)});
        observation_contract.emplace(
            "expected_active_runtime_passes",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_active_runtime_passes)});
        observation_contract.emplace(
            "expected_backdrop_runtime_passes",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_backdrop_runtime_passes)});
        observation_contract.emplace(
            "expected_execution_stages",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_execution_stages)});
        observation_contract.emplace(
            "expected_active_execution_stages",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_active_execution_stages)});
        observation_contract.emplace(
            "expected_backdrop_execution_stages",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_backdrop_execution_stages)});
        observation_contract.emplace(
            "expected_paint_layers",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_paint_layers)});
        observation_contract.emplace(
            "expected_active_paint_layers",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_active_paint_layers)});
        observation_contract.emplace(
            "expected_shadow_paint_layers",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_shadow_paint_layers)});
        observation_contract.emplace(
            "expected_fill_paint_layers",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_fill_paint_layers)});
        observation_contract.emplace(
            "expected_edge_paint_layers",
            json::Value{
                static_cast<std::int64_t>(
                    observation.expected_edge_paint_layers)});
        observation_contract.emplace(
            "max_frame_capture_count",
            json::Value{
                static_cast<std::int64_t>(
                    observation.max_frame_capture_count)});
        observation_contract.emplace(
            "max_frame_capture_pixels",
            json::Value{observation.max_frame_capture_pixels});
        observation_contract.emplace(
            "max_surface_sample_pixels",
            json::Value{observation.max_surface_sample_pixels});
        observation_contract.emplace(
            "max_texture_copy_pixels",
            json::Value{observation.max_texture_copy_pixels});
        observation_contract.emplace(
            "region_name",
            json::Value{observation.region_name});
        observation_contract.emplace(
            "likely_layer",
            json::Value{observation.likely_layer});
        observation_contract.emplace(
            "likely_pass",
            json::Value{observation.likely_pass});

        auto const& audit = plan.execution_audit;
        json::Object execution_audit;
        execution_audit.emplace(
            "schema_version",
            json::Value{static_cast<std::int64_t>(audit.schema_version)});
        execution_audit.emplace(
            "actual_runtime_passes",
            json::Value{
                static_cast<std::int64_t>(audit.actual_runtime_passes)});
        execution_audit.emplace(
            "actual_active_runtime_passes",
            json::Value{static_cast<std::int64_t>(
                audit.actual_active_runtime_passes)});
        execution_audit.emplace(
            "actual_backdrop_runtime_passes",
            json::Value{static_cast<std::int64_t>(
                audit.actual_backdrop_runtime_passes)});
        execution_audit.emplace(
            "actual_execution_stages",
            json::Value{
                static_cast<std::int64_t>(audit.actual_execution_stages)});
        execution_audit.emplace(
            "actual_active_execution_stages",
            json::Value{static_cast<std::int64_t>(
                audit.actual_active_execution_stages)});
        execution_audit.emplace(
            "actual_backdrop_execution_stages",
            json::Value{static_cast<std::int64_t>(
                audit.actual_backdrop_execution_stages)});
        execution_audit.emplace(
            "actual_paint_layers",
            json::Value{
                static_cast<std::int64_t>(audit.actual_paint_layers)});
        execution_audit.emplace(
            "actual_active_paint_layers",
            json::Value{
                static_cast<std::int64_t>(audit.actual_active_paint_layers)});
        execution_audit.emplace(
            "actual_shadow_paint_layers",
            json::Value{
                static_cast<std::int64_t>(audit.actual_shadow_paint_layers)});
        execution_audit.emplace(
            "actual_fill_paint_layers",
            json::Value{
                static_cast<std::int64_t>(audit.actual_fill_paint_layers)});
        execution_audit.emplace(
            "actual_edge_paint_layers",
            json::Value{
                static_cast<std::int64_t>(audit.actual_edge_paint_layers)});
        execution_audit.emplace(
            "runtime_passes_match",
            json::Value{audit.runtime_passes_match});
        execution_audit.emplace(
            "active_runtime_passes_match",
            json::Value{audit.active_runtime_passes_match});
        execution_audit.emplace(
            "backdrop_runtime_passes_match",
            json::Value{audit.backdrop_runtime_passes_match});
        execution_audit.emplace(
            "execution_stages_match",
            json::Value{audit.execution_stages_match});
        execution_audit.emplace(
            "active_execution_stages_match",
            json::Value{audit.active_execution_stages_match});
        execution_audit.emplace(
            "backdrop_execution_stages_match",
            json::Value{audit.backdrop_execution_stages_match});
        execution_audit.emplace(
            "paint_layers_match",
            json::Value{audit.paint_layers_match});
        execution_audit.emplace(
            "active_paint_layers_match",
            json::Value{audit.active_paint_layers_match});
        execution_audit.emplace(
            "shadow_paint_layers_match",
            json::Value{audit.shadow_paint_layers_match});
        execution_audit.emplace(
            "fill_paint_layers_match",
            json::Value{audit.fill_paint_layers_match});
        execution_audit.emplace(
            "edge_paint_layers_match",
            json::Value{audit.edge_paint_layers_match});
        execution_audit.emplace(
            "stage_order_match",
            json::Value{audit.stage_order_match});
        execution_audit.emplace(
            "bounded_texture_copy",
            json::Value{audit.bounded_texture_copy});
        execution_audit.emplace(
            "deterministic_fallback",
            json::Value{audit.deterministic_fallback});
        execution_audit.emplace(
            "contract_satisfied",
            json::Value{audit.contract_satisfied});
        execution_audit.emplace(
            "mismatch_count",
            json::Value{static_cast<std::int64_t>(audit.mismatch_count)});
        execution_audit.emplace(
            "first_mismatch",
            json::Value{audit.first_mismatch ? audit.first_mismatch : "none"});
        execution_audit.emplace(
            "expected_stage_order",
            json::Value{audit.expected_stage_order
                            ? audit.expected_stage_order
                            : "none"});
        execution_audit.emplace(
            "actual_stage_order",
            json::Value{audit.actual_stage_order
                            ? audit.actual_stage_order
                            : "none"});
        execution_audit.emplace(
            "likely_layer",
            json::Value{audit.likely_layer
                            ? audit.likely_layer
                            : "material-execution-contract"});
        execution_audit.emplace(
            "likely_pass",
            json::Value{audit.likely_pass ? audit.likely_pass : "none"});

        json::Object primary_pass;
        primary_pass.emplace("name", json::Value{plan.primary_pass.name});
        primary_pass.emplace("active", json::Value{plan.primary_pass.active});
        primary_pass.emplace(
            "requires_backdrop",
            json::Value{plan.primary_pass.requires_backdrop});
        primary_pass.emplace(
            "sample_taps",
            json::Value{
                static_cast<std::int64_t>(plan.primary_pass.sample_taps)});
        primary_pass.emplace(
            "likely_layer",
            json::Value{plan.primary_pass.likely_layer});
        primary_pass.emplace(
            "executor",
            json::Value{plan.primary_pass.executor});
        primary_pass.emplace(
            "max_texture_copy_pixels",
            json::Value{plan.primary_pass.max_texture_copy_pixels});

        json::Object resource_budget;
        resource_budget.emplace(
            "max_blur_radius",
            json::Value{plan.resource_budget.max_blur_radius});
        resource_budget.emplace(
            "max_sample_taps",
            json::Value{
                static_cast<std::int64_t>(
                    plan.resource_budget.max_sample_taps)});
        resource_budget.emplace(
            "max_sampling_kernel_radius",
            json::Value{
                static_cast<std::int64_t>(
                    plan.resource_budget.max_sampling_kernel_radius)});
        resource_budget.emplace(
            "max_pass_count",
            json::Value{
                static_cast<std::int64_t>(
                    plan.resource_budget.max_pass_count)});
        resource_budget.emplace(
            "max_execution_stages",
            json::Value{
                static_cast<std::int64_t>(
                    plan.resource_budget.max_execution_stages)});
        resource_budget.emplace(
            "max_paint_layers",
            json::Value{
                static_cast<std::int64_t>(
                    plan.resource_budget.max_paint_layers)});
        resource_budget.emplace(
            "max_backdrop_pixels",
            json::Value{plan.resource_budget.max_backdrop_pixels});
        resource_budget.emplace(
            "max_frame_capture_count",
            json::Value{
                static_cast<std::int64_t>(
                    plan.resource_budget.max_frame_capture_count)});
        resource_budget.emplace(
            "max_frame_capture_pixels",
            json::Value{plan.resource_budget.max_frame_capture_pixels});
        resource_budget.emplace(
            "max_surface_sample_pixels",
            json::Value{plan.resource_budget.max_surface_sample_pixels});
        resource_budget.emplace(
            "max_refraction_offset_pixels",
            json::Value{plan.resource_budget.max_refraction_offset_pixels});
        resource_budget.emplace(
            "max_container_spacing",
            json::Value{plan.resource_budget.max_container_spacing});
        resource_budget.emplace(
            "max_paint_layer_inflate",
            json::Value{plan.resource_budget.max_paint_layer_inflate});
        resource_budget.emplace(
            "bounded_texture_copy",
            json::Value{plan.resource_budget.bounded_texture_copy});
        resource_budget.emplace(
            "deterministic_fallback",
            json::Value{plan.resource_budget.deterministic_fallback});

        json::Object backdrop;
        backdrop.emplace("available", json::Value{plan.backdrop.available});
        backdrop.emplace("stable", json::Value{plan.backdrop.stable});
        backdrop.emplace(
            "excludes_foreground_text",
            json::Value{plan.backdrop.excludes_foreground_text});
        backdrop.emplace(
            "color_mean",
            color_to_json(plan.backdrop.color_mean));
        backdrop.emplace(
            "color_sample_count",
            json::Value{static_cast<std::int64_t>(
                plan.backdrop.color_sample_count)});
        backdrop.emplace(
            "color_sample_status",
            json::Value{plan.backdrop.color_sample_status
                            ? plan.backdrop.color_sample_status
                            : "not-sampled"});
        backdrop.emplace("luma_min", json::Value{plan.backdrop.luma_min});
        backdrop.emplace("luma_max", json::Value{plan.backdrop.luma_max});
        backdrop.emplace("luma_mean", json::Value{plan.backdrop.luma_mean});
        backdrop.emplace("luma_span", json::Value{plan.backdrop.luma_span});
        backdrop.emplace(
            "luma_sample_count",
            json::Value{static_cast<std::int64_t>(
                plan.backdrop.luma_sample_count)});
        backdrop.emplace(
            "luma_sample_grid_width",
            json::Value{static_cast<std::int64_t>(
                plan.backdrop.luma_sample_grid_width)});
        backdrop.emplace(
            "luma_sample_grid_height",
            json::Value{static_cast<std::int64_t>(
                plan.backdrop.luma_sample_grid_height)});
        backdrop.emplace(
            "luma_sample_frame",
            json::Value{static_cast<std::int64_t>(
                plan.backdrop.luma_sample_frame)});
        backdrop.emplace(
            "luma_sample_status",
            json::Value{plan.backdrop.luma_sample_status
                            ? plan.backdrop.luma_sample_status
                            : "not-sampled"});
        backdrop.emplace("source", json::Value{plan.backdrop.source});
        backdrop.emplace(
            "luminance_response",
            json::Value{plan.backdrop.luminance_response});
        backdrop.emplace(
            "frosting_response",
            json::Value{plan.backdrop.frosting_response});
        backdrop.emplace(
            "color_response",
            json::Value{plan.backdrop.color_response});
        backdrop.emplace(
            "tint_response",
            json::Value{plan.backdrop.tint_response});
        backdrop.emplace(
            "saturation_response",
            json::Value{plan.backdrop.saturation_response});
        backdrop.emplace(
            "depth_response",
            json::Value{plan.backdrop.depth_response});
        backdrop.emplace(
            "luminance_floor_delta",
            json::Value{plan.backdrop.luminance_floor_delta});
        backdrop.emplace(
            "luminance_gain_delta",
            json::Value{plan.backdrop.luminance_gain_delta});
        backdrop.emplace(
            "edge_highlight_delta",
            json::Value{plan.backdrop.edge_highlight_delta});
        backdrop.emplace(
            "opacity_delta",
            json::Value{plan.backdrop.opacity_delta});
        backdrop.emplace(
            "tint_color_delta",
            json::Value{plan.backdrop.tint_color_delta});
        backdrop.emplace(
            "tint_alpha_delta",
            json::Value{plan.backdrop.tint_alpha_delta});
        backdrop.emplace(
            "saturation_delta",
            json::Value{plan.backdrop.saturation_delta});
        backdrop.emplace(
            "shadow_alpha_delta",
            json::Value{plan.backdrop.shadow_alpha_delta});
        backdrop.emplace(
            "shadow_radius_delta",
            json::Value{plan.backdrop.shadow_radius_delta});
        backdrop.emplace(
            "response_strength",
            json::Value{plan.backdrop.response_strength});

        json::Object backdrop_access;
        backdrop_access.emplace(
            "required",
            json::Value{plan.backdrop_access.required});
        backdrop_access.emplace(
            "stable_required",
            json::Value{plan.backdrop_access.stable_required});
        backdrop_access.emplace(
            "frame_history_required",
            json::Value{plan.backdrop_access.frame_history_required});
        backdrop_access.emplace(
            "shared_frame_capture",
            json::Value{plan.backdrop_access.shared_frame_capture});
        backdrop_access.emplace(
            "next_frame_capture_required",
            json::Value{plan.backdrop_access.next_frame_capture_required});
        backdrop_access.emplace(
            "excludes_foreground_text",
            json::Value{plan.backdrop_access.excludes_foreground_text});
        backdrop_access.emplace(
            "source",
            json::Value{plan.backdrop_access.source});
        backdrop_access.emplace(
            "capture_scope",
            json::Value{plan.backdrop_access.capture_scope});
        backdrop_access.emplace(
            "capture_reason",
            json::Value{plan.backdrop_access.capture_reason});
        backdrop_access.emplace(
            "max_frame_capture_count",
            json::Value{
                static_cast<std::int64_t>(
                    plan.backdrop_access.max_frame_capture_count)});
        backdrop_access.emplace(
            "max_frame_capture_pixels",
            json::Value{plan.backdrop_access.max_frame_capture_pixels});
        backdrop_access.emplace(
            "max_surface_sample_pixels",
            json::Value{plan.backdrop_access.max_surface_sample_pixels});
        backdrop_access.emplace(
            "bounded",
            json::Value{plan.backdrop_access.bounded});

        json::Object theme;
        theme.emplace("source", json::Value{plan.theme.source});
        theme.emplace("profile_name", json::Value{plan.theme.profile_name});
        theme.emplace("token_policy", json::Value{plan.theme.token_policy});
        theme.emplace(
            "foreground",
            color_to_json(plan.theme.foreground));
        theme.emplace(
            "secondary_foreground",
            color_to_json(plan.theme.secondary_foreground));
        theme.emplace(
            "accent_foreground",
            color_to_json(plan.theme.accent_foreground));
        theme.emplace(
            "strong_accent_foreground",
            color_to_json(plan.theme.strong_accent_foreground));
        theme.emplace("tint", color_to_json(plan.theme.tint));
        theme.emplace("border", color_to_json(plan.theme.border));
        theme.emplace(
            "foreground_matches_theme",
            json::Value{plan.theme.foreground_matches_theme});
        theme.emplace(
            "accent_matches_theme",
            json::Value{plan.theme.accent_matches_theme});
        theme.emplace(
            "tint_matches_surface",
            json::Value{plan.theme.tint_matches_surface});
        theme.emplace(
            "border_matches_theme",
            json::Value{plan.theme.border_matches_theme});
        theme.emplace(
            "default_glass_tokens",
            json::Value{plan.theme.default_glass_tokens});

        json::Object foreground_primary;
        foreground_primary.emplace(
            "r",
            json::Value{static_cast<std::int64_t>(plan.foreground.primary.r)});
        foreground_primary.emplace(
            "g",
            json::Value{static_cast<std::int64_t>(plan.foreground.primary.g)});
        foreground_primary.emplace(
            "b",
            json::Value{static_cast<std::int64_t>(plan.foreground.primary.b)});
        foreground_primary.emplace(
            "a",
            json::Value{static_cast<std::int64_t>(plan.foreground.primary.a)});
        json::Object foreground_secondary;
        foreground_secondary.emplace(
            "r",
            json::Value{
                static_cast<std::int64_t>(plan.foreground.secondary.r)});
        foreground_secondary.emplace(
            "g",
            json::Value{
                static_cast<std::int64_t>(plan.foreground.secondary.g)});
        foreground_secondary.emplace(
            "b",
            json::Value{
                static_cast<std::int64_t>(plan.foreground.secondary.b)});
        foreground_secondary.emplace(
            "a",
            json::Value{
                static_cast<std::int64_t>(plan.foreground.secondary.a)});
        json::Object foreground_accent;
        foreground_accent.emplace(
            "r",
            json::Value{static_cast<std::int64_t>(plan.foreground.accent.r)});
        foreground_accent.emplace(
            "g",
            json::Value{static_cast<std::int64_t>(plan.foreground.accent.g)});
        foreground_accent.emplace(
            "b",
            json::Value{static_cast<std::int64_t>(plan.foreground.accent.b)});
        foreground_accent.emplace(
            "a",
            json::Value{static_cast<std::int64_t>(plan.foreground.accent.a)});
        json::Object foreground;
        foreground.emplace(
            "primary",
            json::Value{std::move(foreground_primary)});
        foreground.emplace(
            "secondary",
            json::Value{std::move(foreground_secondary)});
        foreground.emplace(
            "accent",
            json::Value{std::move(foreground_accent)});
        foreground.emplace("scheme", json::Value{plan.foreground.scheme});
        foreground.emplace("source", json::Value{plan.foreground.source});
        foreground.emplace(
            "contrast_policy",
            json::Value{plan.foreground.contrast_policy});
        foreground.emplace(
            "remap_policy",
            json::Value{plan.foreground.remap_policy});
        foreground.emplace(
            "background_luma",
            json::Value{plan.foreground.background_luma});
        foreground.emplace(
            "primary_contrast_ratio",
            json::Value{plan.foreground.primary_contrast_ratio});
        foreground.emplace(
            "secondary_contrast_ratio",
            json::Value{plan.foreground.secondary_contrast_ratio});
        foreground.emplace(
            "accent_contrast_ratio",
            json::Value{plan.foreground.accent_contrast_ratio});
        foreground.emplace(
            "minimum_contrast_ratio",
            json::Value{plan.foreground.minimum_contrast_ratio});
        foreground.emplace(
            "primary_contrast_margin",
            json::Value{plan.foreground.primary_contrast_margin});
        foreground.emplace(
            "secondary_contrast_margin",
            json::Value{plan.foreground.secondary_contrast_margin});
        foreground.emplace(
            "accent_contrast_margin",
            json::Value{plan.foreground.accent_contrast_margin});
        foreground.emplace(
            "backdrop_driven",
            json::Value{plan.foreground.backdrop_driven});
        foreground.emplace(
            "high_contrast",
            json::Value{plan.foreground.high_contrast});
        foreground.emplace(
            "uses_vibrancy",
            json::Value{plan.foreground.uses_vibrancy});
        foreground.emplace(
            "deterministic",
            json::Value{plan.foreground.deterministic});

        json::Object refraction;
        refraction.emplace("model", json::Value{plan.refraction.model});
        refraction.emplace("source", json::Value{plan.refraction.source});
        refraction.emplace("active", json::Value{plan.refraction.active});
        refraction.emplace(
            "backdrop_driven",
            json::Value{plan.refraction.backdrop_driven});
        refraction.emplace(
            "interaction_driven",
            json::Value{plan.refraction.interaction_driven});
        refraction.emplace(
            "reduced_motion_suppressed",
            json::Value{plan.refraction.reduced_motion_suppressed});
        refraction.emplace("bounded", json::Value{plan.refraction.bounded});
        refraction.emplace("strength", json::Value{plan.refraction.strength});
        refraction.emplace("edge_bias", json::Value{plan.refraction.edge_bias});
        refraction.emplace(
            "max_offset_pixels",
            json::Value{plan.refraction.max_offset_pixels});
        refraction.emplace(
            "edge_caustic_intensity",
            json::Value{plan.refraction.edge_caustic_intensity});

        json::Object specular;
        specular.emplace("model", json::Value{plan.specular.model});
        specular.emplace("source", json::Value{plan.specular.source});
        specular.emplace("active", json::Value{plan.specular.active});
        specular.emplace("ambient", json::Value{plan.specular.ambient});
        specular.emplace(
            "interaction_driven",
            json::Value{plan.specular.interaction_driven});
        specular.emplace("bounded", json::Value{plan.specular.bounded});
        specular.emplace("anchor_x", json::Value{plan.specular.anchor_x});
        specular.emplace("anchor_y", json::Value{plan.specular.anchor_y});
        specular.emplace("radius", json::Value{plan.specular.radius});
        specular.emplace("intensity", json::Value{plan.specular.intensity});

        json::Object edge_optics;
        edge_optics.emplace("model", json::Value{plan.edge_optics.model});
        edge_optics.emplace("source", json::Value{plan.edge_optics.source});
        edge_optics.emplace("active", json::Value{plan.edge_optics.active});
        edge_optics.emplace(
            "backdrop_driven",
            json::Value{plan.edge_optics.backdrop_driven});
        edge_optics.emplace(
            "caustic_driven",
            json::Value{plan.edge_optics.caustic_driven});
        edge_optics.emplace("bounded", json::Value{plan.edge_optics.bounded});
        edge_optics.emplace(
            "bevel_width",
            json::Value{plan.edge_optics.bevel_width});
        edge_optics.emplace(
            "inner_highlight",
            json::Value{plan.edge_optics.inner_highlight});
        edge_optics.emplace(
            "outer_shadow",
            json::Value{plan.edge_optics.outer_shadow});
        edge_optics.emplace(
            "chromatic_fringe",
            json::Value{plan.edge_optics.chromatic_fringe});

        json::Object spectral_tint;
        spectral_tint.emplace("model", json::Value{plan.spectral_tint.model});
        spectral_tint.emplace(
            "source",
            json::Value{plan.spectral_tint.source});
        spectral_tint.emplace("active", json::Value{plan.spectral_tint.active});
        spectral_tint.emplace(
            "backdrop_driven",
            json::Value{plan.spectral_tint.backdrop_driven});
        spectral_tint.emplace(
            "color_driven",
            json::Value{plan.spectral_tint.color_driven});
        spectral_tint.emplace(
            "tint_driven",
            json::Value{plan.spectral_tint.tint_driven});
        spectral_tint.emplace(
            "caustic_driven",
            json::Value{plan.spectral_tint.caustic_driven});
        spectral_tint.emplace(
            "bounded",
            json::Value{plan.spectral_tint.bounded});
        spectral_tint.emplace("warmth", json::Value{plan.spectral_tint.warmth});
        spectral_tint.emplace(
            "coolness",
            json::Value{plan.spectral_tint.coolness});
        spectral_tint.emplace(
            "dispersion",
            json::Value{plan.spectral_tint.dispersion});
        spectral_tint.emplace(
            "rim_tint",
            json::Value{plan.spectral_tint.rim_tint});
        spectral_tint.emplace(
            "balance",
            json::Value{plan.spectral_tint.balance});
        spectral_tint.emplace(
            "tint_influence",
            json::Value{plan.spectral_tint.tint_influence});

        json::Object dynamic_lighting;
        dynamic_lighting.emplace(
            "model",
            json::Value{plan.dynamic_lighting.model});
        dynamic_lighting.emplace(
            "source",
            json::Value{plan.dynamic_lighting.source});
        dynamic_lighting.emplace(
            "active",
            json::Value{plan.dynamic_lighting.active});
        dynamic_lighting.emplace(
            "backdrop_driven",
            json::Value{plan.dynamic_lighting.backdrop_driven});
        dynamic_lighting.emplace(
            "color_driven",
            json::Value{plan.dynamic_lighting.color_driven});
        dynamic_lighting.emplace(
            "caustic_driven",
            json::Value{plan.dynamic_lighting.caustic_driven});
        dynamic_lighting.emplace(
            "interaction_driven",
            json::Value{plan.dynamic_lighting.interaction_driven});
        dynamic_lighting.emplace(
            "reduced_motion_suppressed",
            json::Value{plan.dynamic_lighting.reduced_motion_suppressed});
        dynamic_lighting.emplace(
            "bounded",
            json::Value{plan.dynamic_lighting.bounded});
        dynamic_lighting.emplace(
            "direction_x",
            json::Value{plan.dynamic_lighting.direction_x});
        dynamic_lighting.emplace(
            "direction_y",
            json::Value{plan.dynamic_lighting.direction_y});
        dynamic_lighting.emplace(
            "highlight_strength",
            json::Value{plan.dynamic_lighting.highlight_strength});
        dynamic_lighting.emplace(
            "shadow_strength",
            json::Value{plan.dynamic_lighting.shadow_strength});

        json::Object glass_thickness;
        glass_thickness.emplace(
            "model",
            json::Value{plan.glass_thickness.model});
        glass_thickness.emplace(
            "source",
            json::Value{plan.glass_thickness.source});
        glass_thickness.emplace(
            "active",
            json::Value{plan.glass_thickness.active});
        glass_thickness.emplace(
            "size_driven",
            json::Value{plan.glass_thickness.size_driven});
        glass_thickness.emplace(
            "transition_driven",
            json::Value{plan.glass_thickness.transition_driven});
        glass_thickness.emplace(
            "interaction_driven",
            json::Value{plan.glass_thickness.interaction_driven});
        glass_thickness.emplace(
            "reduced_motion_suppressed",
            json::Value{plan.glass_thickness.reduced_motion_suppressed});
        glass_thickness.emplace(
            "bounded",
            json::Value{plan.glass_thickness.bounded});
        glass_thickness.emplace(
            "thickness",
            json::Value{plan.glass_thickness.thickness});
        glass_thickness.emplace(
            "lensing_gain",
            json::Value{plan.glass_thickness.lensing_gain});
        glass_thickness.emplace(
            "shadow_gain",
            json::Value{plan.glass_thickness.shadow_gain});
        glass_thickness.emplace(
            "scattering_gain",
            json::Value{plan.glass_thickness.scattering_gain});

        json::Object glass_dispersion;
        glass_dispersion.emplace(
            "model",
            json::Value{plan.glass_dispersion.model});
        glass_dispersion.emplace(
            "source",
            json::Value{plan.glass_dispersion.source});
        glass_dispersion.emplace(
            "active",
            json::Value{plan.glass_dispersion.active});
        glass_dispersion.emplace(
            "spectral_driven",
            json::Value{plan.glass_dispersion.spectral_driven});
        glass_dispersion.emplace(
            "thickness_driven",
            json::Value{plan.glass_dispersion.thickness_driven});
        glass_dispersion.emplace(
            "transition_driven",
            json::Value{plan.glass_dispersion.transition_driven});
        glass_dispersion.emplace(
            "lighting_driven",
            json::Value{plan.glass_dispersion.lighting_driven});
        glass_dispersion.emplace(
            "interaction_driven",
            json::Value{plan.glass_dispersion.interaction_driven});
        glass_dispersion.emplace(
            "reduced_motion_suppressed",
            json::Value{plan.glass_dispersion.reduced_motion_suppressed});
        glass_dispersion.emplace(
            "bounded",
            json::Value{plan.glass_dispersion.bounded});
        glass_dispersion.emplace(
            "axial_offset_pixels",
            json::Value{plan.glass_dispersion.axial_offset_pixels});
        glass_dispersion.emplace(
            "tangential_offset_pixels",
            json::Value{plan.glass_dispersion.tangential_offset_pixels});
        glass_dispersion.emplace(
            "prismatic_gain",
            json::Value{plan.glass_dispersion.prismatic_gain});
        glass_dispersion.emplace(
            "caustic_spread",
            json::Value{plan.glass_dispersion.caustic_spread});

        json::Object prominent_glass;
        prominent_glass.emplace("model", json::Value{plan.prominent_glass.model});
        prominent_glass.emplace(
            "source",
            json::Value{plan.prominent_glass.source});
        prominent_glass.emplace(
            "active",
            json::Value{plan.prominent_glass.active});
        prominent_glass.emplace(
            "control_driven",
            json::Value{plan.prominent_glass.control_driven});
        prominent_glass.emplace(
            "tint_driven",
            json::Value{plan.prominent_glass.tint_driven});
        prominent_glass.emplace(
            "backdrop_driven",
            json::Value{plan.prominent_glass.backdrop_driven});
        prominent_glass.emplace(
            "interaction_driven",
            json::Value{plan.prominent_glass.interaction_driven});
        prominent_glass.emplace(
            "reduced_motion_suppressed",
            json::Value{plan.prominent_glass.reduced_motion_suppressed});
        prominent_glass.emplace(
            "bounded",
            json::Value{plan.prominent_glass.bounded});
        prominent_glass.emplace(
            "intensity",
            json::Value{plan.prominent_glass.intensity});
        prominent_glass.emplace(
            "tint_weight",
            json::Value{plan.prominent_glass.tint_weight});
        prominent_glass.emplace(
            "edge_lift",
            json::Value{plan.prominent_glass.edge_lift});
        prominent_glass.emplace(
            "lensing_gain",
            json::Value{plan.prominent_glass.lensing_gain});

        json::Object large_surface_legibility;
        large_surface_legibility.emplace(
            "model",
            json::Value{plan.large_surface_legibility.model});
        large_surface_legibility.emplace(
            "source",
            json::Value{plan.large_surface_legibility.source});
        large_surface_legibility.emplace(
            "active",
            json::Value{plan.large_surface_legibility.active});
        large_surface_legibility.emplace(
            "role_driven",
            json::Value{plan.large_surface_legibility.role_driven});
        large_surface_legibility.emplace(
            "size_driven",
            json::Value{plan.large_surface_legibility.size_driven});
        large_surface_legibility.emplace(
            "backdrop_driven",
            json::Value{plan.large_surface_legibility.backdrop_driven});
        large_surface_legibility.emplace(
            "contrast_driven",
            json::Value{plan.large_surface_legibility.contrast_driven});
        large_surface_legibility.emplace(
            "brightness_driven",
            json::Value{plan.large_surface_legibility.brightness_driven});
        large_surface_legibility.emplace(
            "bounded",
            json::Value{plan.large_surface_legibility.bounded});
        large_surface_legibility.emplace(
            "response_strength",
            json::Value{plan.large_surface_legibility.response_strength});
        large_surface_legibility.emplace(
            "opacity_delta",
            json::Value{plan.large_surface_legibility.opacity_delta});
        large_surface_legibility.emplace(
            "tint_alpha_delta",
            json::Value{plan.large_surface_legibility.tint_alpha_delta});
        large_surface_legibility.emplace(
            "luminance_floor_delta",
            json::Value{plan.large_surface_legibility.luminance_floor_delta});
        large_surface_legibility.emplace(
            "edge_highlight_delta",
            json::Value{plan.large_surface_legibility.edge_highlight_delta});
        large_surface_legibility.emplace(
            "shadow_alpha_delta",
            json::Value{plan.large_surface_legibility.shadow_alpha_delta});
        large_surface_legibility.emplace(
            "shadow_radius_delta",
            json::Value{plan.large_surface_legibility.shadow_radius_delta});

        json::Object interaction;
        interaction.emplace("enabled", json::Value{plan.interaction.enabled});
        interaction.emplace("active", json::Value{plan.interaction.active});
        interaction.emplace("hovered", json::Value{plan.interaction.hovered});
        interaction.emplace("pressed", json::Value{plan.interaction.pressed});
        interaction.emplace("focused", json::Value{plan.interaction.focused});
        interaction.emplace(
            "pointer_inside",
            json::Value{plan.interaction.pointer_inside});
        interaction.emplace(
            "reduce_motion",
            json::Value{plan.interaction.reduce_motion});
        interaction.emplace("pointer_x", json::Value{plan.interaction.pointer_x});
        interaction.emplace("pointer_y", json::Value{plan.interaction.pointer_y});
        interaction.emplace(
            "response_strength",
            json::Value{plan.interaction.response_strength});
        interaction.emplace(
            "opacity_delta",
            json::Value{plan.interaction.opacity_delta});
        interaction.emplace(
            "blur_radius_delta",
            json::Value{plan.interaction.blur_radius_delta});
        interaction.emplace(
            "saturation_delta",
            json::Value{plan.interaction.saturation_delta});
        interaction.emplace(
            "edge_highlight_delta",
            json::Value{plan.interaction.edge_highlight_delta});
        interaction.emplace(
            "shadow_alpha_delta",
            json::Value{plan.interaction.shadow_alpha_delta});
        interaction.emplace(
            "shadow_radius_delta",
            json::Value{plan.interaction.shadow_radius_delta});
        interaction.emplace("state", json::Value{plan.interaction.state});
        interaction.emplace(
            "enablement_reason",
            json::Value{plan.interaction.enablement_reason});
        interaction.emplace(
            "response_model",
            json::Value{plan.interaction.response_model});
        interaction.emplace(
            "motion_policy",
            json::Value{plan.interaction.motion_policy});
        interaction.emplace(
            "specular_model",
            json::Value{plan.interaction.specular_model});
        interaction.emplace(
            "specular_highlight_active",
            json::Value{plan.interaction.specular_highlight_active});
        interaction.emplace(
            "specular_anchor_x",
            json::Value{plan.interaction.specular_anchor_x});
        interaction.emplace(
            "specular_anchor_y",
            json::Value{plan.interaction.specular_anchor_y});
        interaction.emplace(
            "specular_radius",
            json::Value{plan.interaction.specular_radius});
        interaction.emplace(
            "specular_intensity",
            json::Value{plan.interaction.specular_intensity});
        interaction.emplace(
            "pointer_lens_model",
            json::Value{plan.interaction.pointer_lens_model});
        interaction.emplace(
            "pointer_lens_active",
            json::Value{plan.interaction.pointer_lens_active});
        interaction.emplace(
            "pointer_lens_anchor_x",
            json::Value{plan.interaction.pointer_lens_anchor_x});
        interaction.emplace(
            "pointer_lens_anchor_y",
            json::Value{plan.interaction.pointer_lens_anchor_y});
        interaction.emplace(
            "pointer_lens_radius",
            json::Value{plan.interaction.pointer_lens_radius});
        interaction.emplace(
            "pointer_lens_strength",
            json::Value{plan.interaction.pointer_lens_strength});
        interaction.emplace(
            "control_morph_model",
            json::Value{plan.interaction.control_morph_model});
        interaction.emplace(
            "control_morph_active",
            json::Value{plan.interaction.control_morph_active});
        interaction.emplace(
            "control_morph_reduced_motion_suppressed",
            json::Value{
                plan.interaction.control_morph_reduced_motion_suppressed});
        interaction.emplace(
            "control_morph_scale_delta",
            json::Value{plan.interaction.control_morph_scale_delta});
        interaction.emplace(
            "control_morph_depth",
            json::Value{plan.interaction.control_morph_depth});
        interaction.emplace(
            "control_morph_edge",
            json::Value{plan.interaction.control_morph_edge});
        interaction.emplace(
            "control_morph_shadow",
            json::Value{plan.interaction.control_morph_shadow});
        interaction.emplace(
            "deterministic",
            json::Value{plan.interaction.deterministic});

        json::Object optical_response;
        optical_response.emplace(
            "response_model",
            json::Value{plan.optical_response.response_model});
        optical_response.emplace(
            "blur_strategy",
            json::Value{plan.optical_response.blur_strategy});
        optical_response.emplace(
            "color_strategy",
            json::Value{plan.optical_response.color_strategy});
        optical_response.emplace(
            "depth_strategy",
            json::Value{plan.optical_response.depth_strategy});
        optical_response.emplace(
            "backdrop_driven",
            json::Value{plan.optical_response.backdrop_driven});
        optical_response.emplace(
            "blur_active",
            json::Value{plan.optical_response.blur_active});
        optical_response.emplace(
            "frosting_active",
            json::Value{plan.optical_response.frosting_active});
        optical_response.emplace(
            "tint_active",
            json::Value{plan.optical_response.tint_active});
        optical_response.emplace(
            "saturation_active",
            json::Value{plan.optical_response.saturation_active});
        optical_response.emplace(
            "luminance_preservation_active",
            json::Value{
                plan.optical_response.luminance_preservation_active});
        optical_response.emplace(
            "edge_highlight_active",
            json::Value{plan.optical_response.edge_highlight_active});
        optical_response.emplace(
            "depth_shadow_active",
            json::Value{plan.optical_response.depth_shadow_active});
        optical_response.emplace(
            "noise_dither_active",
            json::Value{plan.optical_response.noise_dither_active});
        optical_response.emplace(
            "refraction_active",
            json::Value{plan.optical_response.refraction_active});
        optical_response.emplace(
            "spectral_tint_active",
            json::Value{plan.optical_response.spectral_tint_active});
        optical_response.emplace(
            "dynamic_lighting_active",
            json::Value{plan.optical_response.dynamic_lighting_active});
        optical_response.emplace(
            "glass_thickness_active",
            json::Value{plan.optical_response.glass_thickness_active});
        optical_response.emplace(
            "glass_dispersion_active",
            json::Value{plan.optical_response.glass_dispersion_active});
        optical_response.emplace(
            "scroll_edge_active",
            json::Value{plan.optical_response.scroll_edge_active});
        optical_response.emplace(
            "prominent_glass_active",
            json::Value{plan.optical_response.prominent_glass_active});
        optical_response.emplace(
            "clear_glass_legibility_active",
            json::Value{
                plan.optical_response.clear_glass_legibility_active});
        optical_response.emplace(
            "large_surface_legibility_active",
            json::Value{
                plan.optical_response.large_surface_legibility_active});
        optical_response.emplace(
            "foreground_vibrancy_active",
            json::Value{plan.optical_response.foreground_vibrancy_active});
        optical_response.emplace(
            "interaction_active",
            json::Value{plan.optical_response.interaction_active});
        optical_response.emplace(
            "interaction_modulates_optics",
            json::Value{plan.optical_response.interaction_modulates_optics});
        optical_response.emplace(
            "deterministic_fallback",
            json::Value{plan.optical_response.deterministic_fallback});

        auto const& composition = plan.optical_composition;
        json::Object optical_composition;
        optical_composition.emplace(
            "schema_version",
            json::Value{
                static_cast<std::int64_t>(composition.schema_version)});
        optical_composition.emplace("model", json::Value{composition.model});
        optical_composition.emplace(
            "blur_source",
            json::Value{composition.blur_source});
        optical_composition.emplace(
            "frosting_source",
            json::Value{composition.frosting_source});
        optical_composition.emplace(
            "tint_source",
            json::Value{composition.tint_source});
        optical_composition.emplace(
            "luminance_source",
            json::Value{composition.luminance_source});
        optical_composition.emplace(
            "depth_source",
            json::Value{composition.depth_source});
        optical_composition.emplace(
            "refraction_source",
            json::Value{composition.refraction_source});
        optical_composition.emplace(
            "spectral_tint_source",
            json::Value{composition.spectral_tint_source});
        optical_composition.emplace(
            "dynamic_lighting_source",
            json::Value{composition.dynamic_lighting_source});
        optical_composition.emplace(
            "glass_thickness_source",
            json::Value{composition.glass_thickness_source});
        optical_composition.emplace(
            "glass_dispersion_source",
            json::Value{composition.glass_dispersion_source});
        optical_composition.emplace(
            "scroll_edge_source",
            json::Value{composition.scroll_edge_source});
        optical_composition.emplace(
            "prominent_glass_source",
            json::Value{composition.prominent_glass_source});
        optical_composition.emplace(
            "clear_glass_legibility_source",
            json::Value{composition.clear_glass_legibility_source});
        optical_composition.emplace(
            "large_surface_legibility_source",
            json::Value{composition.large_surface_legibility_source});
        optical_composition.emplace(
            "interaction_source",
            json::Value{composition.interaction_source});
        optical_composition.emplace(
            "transition_source",
            json::Value{composition.transition_source});
        optical_composition.emplace(
            "fallback_source",
            json::Value{composition.fallback_source});
        optical_composition.emplace(
            "stage_order",
            json::Value{composition.stage_order});
        optical_composition.emplace(
            "backdrop_capture_policy",
            json::Value{composition.backdrop_capture_policy});
        optical_composition.emplace(
            "foreground_sampling_policy",
            json::Value{composition.foreground_sampling_policy});
        optical_composition.emplace(
            "backdrop_sampled",
            json::Value{composition.backdrop_sampled});
        optical_composition.emplace(
            "blur_required",
            json::Value{composition.blur_required});
        optical_composition.emplace(
            "frosting_required",
            json::Value{composition.frosting_required});
        optical_composition.emplace(
            "tint_required",
            json::Value{composition.tint_required});
        optical_composition.emplace(
            "saturation_required",
            json::Value{composition.saturation_required});
        optical_composition.emplace(
            "luminance_required",
            json::Value{composition.luminance_required});
        optical_composition.emplace(
            "edge_required",
            json::Value{composition.edge_required});
        optical_composition.emplace(
            "shadow_required",
            json::Value{composition.shadow_required});
        optical_composition.emplace(
            "noise_required",
            json::Value{composition.noise_required});
        optical_composition.emplace(
            "refraction_required",
            json::Value{composition.refraction_required});
        optical_composition.emplace(
            "spectral_tint_required",
            json::Value{composition.spectral_tint_required});
        optical_composition.emplace(
            "dynamic_lighting_required",
            json::Value{composition.dynamic_lighting_required});
        optical_composition.emplace(
            "glass_thickness_required",
            json::Value{composition.glass_thickness_required});
        optical_composition.emplace(
            "glass_dispersion_required",
            json::Value{composition.glass_dispersion_required});
        optical_composition.emplace(
            "scroll_edge_required",
            json::Value{composition.scroll_edge_required});
        optical_composition.emplace(
            "prominent_glass_required",
            json::Value{composition.prominent_glass_required});
        optical_composition.emplace(
            "clear_glass_legibility_required",
            json::Value{composition.clear_glass_legibility_required});
        optical_composition.emplace(
            "large_surface_legibility_required",
            json::Value{composition.large_surface_legibility_required});
        optical_composition.emplace(
            "interaction_required",
            json::Value{composition.interaction_required});
        optical_composition.emplace(
            "transition_required",
            json::Value{composition.transition_required});
        optical_composition.emplace(
            "fallback_required",
            json::Value{composition.fallback_required});
        optical_composition.emplace(
            "backdrop_capture_required",
            json::Value{composition.backdrop_capture_required});
        optical_composition.emplace(
            "foreground_excluded_from_backdrop",
            json::Value{composition.foreground_excluded_from_backdrop});
        optical_composition.emplace(
            "stage_order_stable",
            json::Value{composition.stage_order_stable});
        optical_composition.emplace("bounded", json::Value{composition.bounded});
        optical_composition.emplace(
            "deterministic",
            json::Value{composition.deterministic});
        optical_composition.emplace("opacity", json::Value{composition.opacity});
        optical_composition.emplace(
            "blur_radius",
            json::Value{composition.blur_radius});
        optical_composition.emplace(
            "tint_alpha",
            json::Value{composition.tint_alpha});
        optical_composition.emplace(
            "saturation",
            json::Value{composition.saturation});
        optical_composition.emplace(
            "luminance_floor",
            json::Value{composition.luminance_floor});
        optical_composition.emplace(
            "luminance_gain",
            json::Value{composition.luminance_gain});
        optical_composition.emplace(
            "edge_highlight",
            json::Value{composition.edge_highlight});
        optical_composition.emplace(
            "edge_width",
            json::Value{composition.edge_width});
        optical_composition.emplace(
            "noise_opacity",
            json::Value{composition.noise_opacity});
        optical_composition.emplace(
            "shadow_alpha",
            json::Value{composition.shadow_alpha});
        optical_composition.emplace(
            "shadow_radius",
            json::Value{composition.shadow_radius});
        optical_composition.emplace(
            "refraction_strength",
            json::Value{composition.refraction_strength});
        optical_composition.emplace(
            "refraction_edge_bias",
            json::Value{composition.refraction_edge_bias});
        optical_composition.emplace(
            "refraction_offset_pixels",
            json::Value{composition.refraction_offset_pixels});
        optical_composition.emplace(
            "refraction_edge_caustic_intensity",
            json::Value{
                composition.refraction_edge_caustic_intensity});
        optical_composition.emplace(
            "edge_bevel_width",
            json::Value{composition.edge_bevel_width});
        optical_composition.emplace(
            "edge_inner_highlight",
            json::Value{composition.edge_inner_highlight});
        optical_composition.emplace(
            "edge_outer_shadow",
            json::Value{composition.edge_outer_shadow});
        optical_composition.emplace(
            "edge_chromatic_fringe",
            json::Value{composition.edge_chromatic_fringe});
        optical_composition.emplace(
            "spectral_tint_warmth",
            json::Value{composition.spectral_tint_warmth});
        optical_composition.emplace(
            "spectral_tint_coolness",
            json::Value{composition.spectral_tint_coolness});
        optical_composition.emplace(
            "spectral_dispersion",
            json::Value{composition.spectral_dispersion});
        optical_composition.emplace(
            "spectral_rim_tint",
            json::Value{composition.spectral_rim_tint});
        optical_composition.emplace(
            "spectral_tint_balance",
            json::Value{composition.spectral_tint_balance});
        optical_composition.emplace(
            "spectral_tint_influence",
            json::Value{composition.spectral_tint_influence});
        optical_composition.emplace(
            "dynamic_light_direction_x",
            json::Value{composition.dynamic_light_direction_x});
        optical_composition.emplace(
            "dynamic_light_direction_y",
            json::Value{composition.dynamic_light_direction_y});
        optical_composition.emplace(
            "dynamic_light_highlight",
            json::Value{composition.dynamic_light_highlight});
        optical_composition.emplace(
            "dynamic_light_shadow",
            json::Value{composition.dynamic_light_shadow});
        optical_composition.emplace(
            "glass_thickness",
            json::Value{composition.glass_thickness});
        optical_composition.emplace(
            "glass_lensing_gain",
            json::Value{composition.glass_lensing_gain});
        optical_composition.emplace(
            "glass_shadow_gain",
            json::Value{composition.glass_shadow_gain});
        optical_composition.emplace(
            "glass_scattering_gain",
            json::Value{composition.glass_scattering_gain});
        optical_composition.emplace(
            "glass_dispersion_axial_offset",
            json::Value{composition.glass_dispersion_axial_offset});
        optical_composition.emplace(
            "glass_dispersion_tangential_offset",
            json::Value{composition.glass_dispersion_tangential_offset});
        optical_composition.emplace(
            "glass_dispersion_prismatic_gain",
            json::Value{composition.glass_dispersion_prismatic_gain});
        optical_composition.emplace(
            "glass_dispersion_caustic_spread",
            json::Value{composition.glass_dispersion_caustic_spread});
        optical_composition.emplace(
            "scroll_edge_extent",
            json::Value{composition.scroll_edge_extent});
        optical_composition.emplace(
            "scroll_edge_dissolve",
            json::Value{composition.scroll_edge_dissolve});
        optical_composition.emplace(
            "scroll_edge_dimming",
            json::Value{composition.scroll_edge_dimming});
        optical_composition.emplace(
            "scroll_edge_hard_style",
            json::Value{composition.scroll_edge_hard_style});
        optical_composition.emplace(
            "prominent_glass_intensity",
            json::Value{composition.prominent_glass_intensity});
        optical_composition.emplace(
            "prominent_glass_tint_weight",
            json::Value{composition.prominent_glass_tint_weight});
        optical_composition.emplace(
            "prominent_glass_edge_lift",
            json::Value{composition.prominent_glass_edge_lift});
        optical_composition.emplace(
            "prominent_glass_lensing_gain",
            json::Value{composition.prominent_glass_lensing_gain});
        optical_composition.emplace(
            "clear_glass_dimming",
            json::Value{composition.clear_glass_dimming});
        optical_composition.emplace(
            "clear_glass_contrast",
            json::Value{composition.clear_glass_contrast});
        optical_composition.emplace(
            "clear_glass_brightness_response",
            json::Value{composition.clear_glass_brightness_response});
        optical_composition.emplace(
            "clear_glass_detail_response",
            json::Value{composition.clear_glass_detail_response});
        optical_composition.emplace(
            "large_surface_legibility_response",
            json::Value{composition.large_surface_legibility_response});
        optical_composition.emplace(
            "large_surface_opacity_delta",
            json::Value{composition.large_surface_opacity_delta});
        optical_composition.emplace(
            "large_surface_tint_alpha_delta",
            json::Value{composition.large_surface_tint_alpha_delta});
        optical_composition.emplace(
            "large_surface_luminance_floor_delta",
            json::Value{composition.large_surface_luminance_floor_delta});
        optical_composition.emplace(
            "large_surface_edge_highlight_delta",
            json::Value{composition.large_surface_edge_highlight_delta});
        optical_composition.emplace(
            "interaction_response_strength",
            json::Value{composition.interaction_response_strength});
        optical_composition.emplace(
            "interaction_control_morph_scale_delta",
            json::Value{
                composition.interaction_control_morph_scale_delta});
        optical_composition.emplace(
            "interaction_control_morph_depth",
            json::Value{composition.interaction_control_morph_depth});
        optical_composition.emplace(
            "interaction_control_morph_edge",
            json::Value{composition.interaction_control_morph_edge});
        optical_composition.emplace(
            "interaction_control_morph_shadow",
            json::Value{composition.interaction_control_morph_shadow});
        optical_composition.emplace(
            "transition_progress",
            json::Value{composition.transition_progress});
        optical_composition.emplace(
            "transition_opacity_gain",
            json::Value{composition.transition_opacity_gain});
        optical_composition.emplace(
            "transition_optical_gain",
            json::Value{composition.transition_optical_gain});
        optical_composition.emplace(
            "transition_refraction_gain",
            json::Value{composition.transition_refraction_gain});
        optical_composition.emplace(
            "transition_materialize_wave_strength",
            json::Value{
                composition.transition_materialize_wave_strength});
        optical_composition.emplace(
            "transition_materialize_edge_lift",
            json::Value{composition.transition_materialize_edge_lift});
        optical_composition.emplace(
            "transition_materialize_lensing_gain",
            json::Value{
                composition.transition_materialize_lensing_gain});
        optical_composition.emplace(
            "transition_materialize_rim_position",
            json::Value{
                composition.transition_materialize_rim_position});
        optical_composition.emplace(
            "sample_taps",
            json::Value{
                static_cast<std::int64_t>(composition.sample_taps)});
        optical_composition.emplace(
            "max_texture_copy_pixels",
            json::Value{composition.max_texture_copy_pixels});
        optical_composition.emplace(
            "max_surface_sample_pixels",
            json::Value{composition.max_surface_sample_pixels});

        json::Object quality_policy;
        quality_policy.emplace(
            "allow_backdrop_sampling",
            json::Value{plan.quality_policy.allow_backdrop_sampling});
        quality_policy.emplace(
            "allow_noise",
            json::Value{plan.quality_policy.allow_noise});
        quality_policy.emplace(
            "allow_shadow",
            json::Value{plan.quality_policy.allow_shadow});
        quality_policy.emplace(
            "max_blur_radius",
            json::Value{plan.quality_policy.max_blur_radius});
        quality_policy.emplace(
            "max_sample_taps",
            json::Value{
                static_cast<std::int64_t>(
                    plan.quality_policy.max_sample_taps)});
        quality_policy.emplace(
            "max_backdrop_pixels",
            json::Value{plan.quality_policy.max_backdrop_pixels});

        json::Object sampling_kernel;
        sampling_kernel.emplace("name", json::Value{plan.sampling_kernel.name});
        sampling_kernel.emplace(
            "radius",
            json::Value{
                static_cast<std::int64_t>(plan.sampling_kernel.radius)});
        sampling_kernel.emplace(
            "sample_taps",
            json::Value{
                static_cast<std::int64_t>(
                    plan.sampling_kernel.sample_taps)});
        sampling_kernel.emplace(
            "blur_step_scale",
            json::Value{plan.sampling_kernel.blur_step_scale});
        sampling_kernel.emplace(
            "weight_profile",
            json::Value{plan.sampling_kernel.weight_profile});
        sampling_kernel.emplace(
            "requires_backdrop",
            json::Value{plan.sampling_kernel.requires_backdrop});
        sampling_kernel.emplace(
            "bounded",
            json::Value{plan.sampling_kernel.bounded});

        json::Object luminance_curve;
        luminance_curve.emplace(
            "name",
            json::Value{plan.luminance_curve.name});
        luminance_curve.emplace(
            "floor",
            json::Value{plan.luminance_curve.floor});
        luminance_curve.emplace(
            "gain",
            json::Value{plan.luminance_curve.gain});
        luminance_curve.emplace(
            "gamma",
            json::Value{plan.luminance_curve.gamma});
        luminance_curve.emplace(
            "midpoint",
            json::Value{plan.luminance_curve.midpoint});
        luminance_curve.emplace(
            "contrast",
            json::Value{plan.luminance_curve.contrast});
        luminance_curve.emplace(
            "edge_lift",
            json::Value{plan.luminance_curve.edge_lift});
        luminance_curve.emplace(
            "backdrop_driven",
            json::Value{plan.luminance_curve.backdrop_driven});
        luminance_curve.emplace(
            "bounded",
            json::Value{plan.luminance_curve.bounded});

        json::Array passes;
        {
            json::Object pass;
            pass.emplace("name", json::Value{plan.primary_pass.name});
            pass.emplace("active", json::Value{plan.primary_pass.active});
            pass.emplace(
                "requires_backdrop",
                json::Value{plan.primary_pass.requires_backdrop});
            pass.emplace(
                "likely_layer",
                json::Value{plan.primary_pass.likely_layer});
            pass.emplace(
                "sample_taps",
                json::Value{
                    static_cast<std::int64_t>(
                        plan.primary_pass.sample_taps)});
            pass.emplace(
                "executor",
                json::Value{plan.primary_pass.executor});
            pass.emplace(
                "max_texture_copy_pixels",
                json::Value{plan.primary_pass.max_texture_copy_pixels});
            passes.push_back(json::Value{std::move(pass)});
        }

        json::Array execution_stages;
        for (unsigned int i = 0; i < plan.execution_stage_count; ++i) {
            auto const& stage = plan.execution_stages[i];
            auto const& optics = stage.optics;
            json::Object optics_json;
            optics_json.emplace("channel", json::Value{optics.channel});
            optics_json.emplace("opacity", json::Value{optics.opacity});
            optics_json.emplace("blur_radius", json::Value{optics.blur_radius});
            optics_json.emplace("tint_alpha", json::Value{optics.tint_alpha});
            optics_json.emplace("saturation", json::Value{optics.saturation});
            optics_json.emplace(
                "luminance_floor",
                json::Value{optics.luminance_floor});
            optics_json.emplace(
                "luminance_gain",
                json::Value{optics.luminance_gain});
            optics_json.emplace(
                "edge_highlight",
                json::Value{optics.edge_highlight});
            optics_json.emplace("edge_width", json::Value{optics.edge_width});
            optics_json.emplace(
                "noise_opacity",
                json::Value{optics.noise_opacity});
            optics_json.emplace(
                "shadow_alpha",
                json::Value{optics.shadow_alpha});
            optics_json.emplace(
                "shadow_radius",
                json::Value{optics.shadow_radius});
            optics_json.emplace(
                "specular_model",
                json::Value{optics.specular_model});
            optics_json.emplace(
                "specular_anchor_x",
                json::Value{optics.specular_anchor_x});
            optics_json.emplace(
                "specular_anchor_y",
                json::Value{optics.specular_anchor_y});
            optics_json.emplace(
                "specular_radius",
                json::Value{optics.specular_radius});
            optics_json.emplace(
                "specular_intensity",
                json::Value{optics.specular_intensity});
            optics_json.emplace(
                "pointer_lens_model",
                json::Value{optics.pointer_lens_model});
            optics_json.emplace(
                "pointer_lens_anchor_x",
                json::Value{optics.pointer_lens_anchor_x});
            optics_json.emplace(
                "pointer_lens_anchor_y",
                json::Value{optics.pointer_lens_anchor_y});
            optics_json.emplace(
                "pointer_lens_radius",
                json::Value{optics.pointer_lens_radius});
            optics_json.emplace(
                "pointer_lens_strength",
                json::Value{optics.pointer_lens_strength});
            optics_json.emplace(
                "control_morph_model",
                json::Value{optics.control_morph_model});
            optics_json.emplace(
                "control_morph_scale_delta",
                json::Value{optics.control_morph_scale_delta});
            optics_json.emplace(
                "control_morph_depth",
                json::Value{optics.control_morph_depth});
            optics_json.emplace(
                "control_morph_edge",
                json::Value{optics.control_morph_edge});
            optics_json.emplace(
                "control_morph_shadow",
                json::Value{optics.control_morph_shadow});
            optics_json.emplace(
                "prominent_glass_model",
                json::Value{optics.prominent_glass_model});
            optics_json.emplace(
                "prominent_glass_intensity",
                json::Value{optics.prominent_glass_intensity});
            optics_json.emplace(
                "prominent_glass_tint_weight",
                json::Value{optics.prominent_glass_tint_weight});
            optics_json.emplace(
                "prominent_glass_edge_lift",
                json::Value{optics.prominent_glass_edge_lift});
            optics_json.emplace(
                "prominent_glass_lensing_gain",
                json::Value{optics.prominent_glass_lensing_gain});
            optics_json.emplace(
                "refraction_model",
                json::Value{optics.refraction_model});
            optics_json.emplace(
                "refraction_strength",
                json::Value{optics.refraction_strength});
            optics_json.emplace(
                "refraction_edge_bias",
                json::Value{optics.refraction_edge_bias});
            optics_json.emplace(
                "refraction_offset_pixels",
                json::Value{optics.refraction_offset_pixels});
            optics_json.emplace(
                "refraction_edge_caustic_intensity",
                json::Value{
                    optics.refraction_edge_caustic_intensity});
            optics_json.emplace(
                "edge_optics_model",
                json::Value{optics.edge_optics_model});
            optics_json.emplace(
                "edge_bevel_width",
                json::Value{optics.edge_bevel_width});
            optics_json.emplace(
                "edge_inner_highlight",
                json::Value{optics.edge_inner_highlight});
            optics_json.emplace(
                "edge_outer_shadow",
                json::Value{optics.edge_outer_shadow});
            optics_json.emplace(
                "edge_chromatic_fringe",
                json::Value{optics.edge_chromatic_fringe});
            optics_json.emplace(
                "spectral_tint_model",
                json::Value{optics.spectral_tint_model});
            optics_json.emplace(
                "spectral_tint_warmth",
                json::Value{optics.spectral_tint_warmth});
            optics_json.emplace(
                "spectral_tint_coolness",
                json::Value{optics.spectral_tint_coolness});
            optics_json.emplace(
                "spectral_dispersion",
                json::Value{optics.spectral_dispersion});
            optics_json.emplace(
                "spectral_rim_tint",
                json::Value{optics.spectral_rim_tint});
            optics_json.emplace(
                "dynamic_lighting_model",
                json::Value{optics.dynamic_lighting_model});
            optics_json.emplace(
                "dynamic_light_direction_x",
                json::Value{optics.dynamic_light_direction_x});
            optics_json.emplace(
                "dynamic_light_direction_y",
                json::Value{optics.dynamic_light_direction_y});
            optics_json.emplace(
                "dynamic_light_highlight",
                json::Value{optics.dynamic_light_highlight});
            optics_json.emplace(
                "dynamic_light_shadow",
                json::Value{optics.dynamic_light_shadow});
            optics_json.emplace(
                "glass_thickness_model",
                json::Value{optics.glass_thickness_model});
            optics_json.emplace(
                "glass_thickness",
                json::Value{optics.glass_thickness});
            optics_json.emplace(
                "glass_lensing_gain",
                json::Value{optics.glass_lensing_gain});
            optics_json.emplace(
                "glass_shadow_gain",
                json::Value{optics.glass_shadow_gain});
            optics_json.emplace(
                "glass_scattering_gain",
                json::Value{optics.glass_scattering_gain});
            optics_json.emplace(
                "glass_dispersion_model",
                json::Value{optics.glass_dispersion_model});
            optics_json.emplace(
                "glass_dispersion_axial_offset",
                json::Value{optics.glass_dispersion_axial_offset});
            optics_json.emplace(
                "glass_dispersion_tangential_offset",
                json::Value{optics.glass_dispersion_tangential_offset});
            optics_json.emplace(
                "glass_dispersion_prismatic_gain",
                json::Value{optics.glass_dispersion_prismatic_gain});
            optics_json.emplace(
                "glass_dispersion_caustic_spread",
                json::Value{optics.glass_dispersion_caustic_spread});
            optics_json.emplace(
                "scroll_edge_model",
                json::Value{optics.scroll_edge_model});
            optics_json.emplace(
                "scroll_edge_extent",
                json::Value{optics.scroll_edge_extent});
            optics_json.emplace(
                "scroll_edge_dissolve",
                json::Value{optics.scroll_edge_dissolve});
            optics_json.emplace(
                "scroll_edge_dimming",
                json::Value{optics.scroll_edge_dimming});
            optics_json.emplace(
                "scroll_edge_hard_style",
                json::Value{optics.scroll_edge_hard_style});
            optics_json.emplace(
                "clear_glass_legibility_model",
                json::Value{optics.clear_glass_legibility_model});
            optics_json.emplace(
                "clear_glass_dimming",
                json::Value{optics.clear_glass_dimming});
            optics_json.emplace(
                "clear_glass_contrast",
                json::Value{optics.clear_glass_contrast});
            optics_json.emplace(
                "clear_glass_brightness_response",
                json::Value{optics.clear_glass_brightness_response});
            optics_json.emplace(
                "clear_glass_detail_response",
                json::Value{optics.clear_glass_detail_response});
            json::Object out_stage;
            out_stage.emplace("name", json::Value{stage.name});
            out_stage.emplace("active", json::Value{stage.active});
            out_stage.emplace(
                "requires_backdrop",
                json::Value{stage.requires_backdrop});
            out_stage.emplace(
                "likely_layer",
                json::Value{stage.likely_layer});
            out_stage.emplace(
                "sample_taps",
                json::Value{
                    static_cast<std::int64_t>(stage.sample_taps)});
            out_stage.emplace(
                "executor",
                json::Value{stage.executor});
            out_stage.emplace(
                "max_texture_copy_pixels",
                json::Value{stage.max_texture_copy_pixels});
            out_stage.emplace("bounded", json::Value{stage.bounded});
            out_stage.emplace("optics", json::Value{std::move(optics_json)});
            execution_stages.push_back(json::Value{std::move(out_stage)});
        }

        json::Array paint_layers;
        for (unsigned int i = 0; i < plan.paint_layer_count; ++i) {
            auto const& layer = plan.paint_layers[i];
            json::Object layer_json;
            layer_json.emplace("name", json::Value{layer.name});
            layer_json.emplace("active", json::Value{layer.active});
            layer_json.emplace("executor", json::Value{layer.executor});
            layer_json.emplace(
                "background_effect",
                json::Value{layer.background_effect});
            layer_json.emplace("x_offset", json::Value{layer.x_offset});
            layer_json.emplace("y_offset", json::Value{layer.y_offset});
            layer_json.emplace("inflate", json::Value{layer.inflate});
            layer_json.emplace("radius_delta", json::Value{layer.radius_delta});
            layer_json.emplace("stroke_width", json::Value{layer.stroke_width});
            layer_json.emplace("color", color_to_json(layer.color));
            layer_json.emplace("opacity", json::Value{layer.opacity});
            layer_json.emplace(
                "soft_edge_radius",
                json::Value{layer.soft_edge_radius});
            layer_json.emplace("bounded", json::Value{layer.bounded});
            paint_layers.push_back(json::Value{std::move(layer_json)});
        }

        json::Object out;
        out.emplace(
            "command_index",
            json::Value{static_cast<std::int64_t>(record.command_index)});
        out.emplace(
            "contract_version",
            json::Value{
                static_cast<std::int64_t>(plan.contract_version)});
        out.emplace("kind", json::Value{material_kind_name(plan.kind)});
        out.emplace(
            "role",
            json::Value{material_surface_role_name(plan.role)});
        out.emplace(
            "container",
            material_container_analysis_json(plan.container));
        out.emplace(
            "transition",
            material_transition_analysis_json(plan.transition));
        out.emplace(
            "glass_identity",
            material_glass_identity_analysis_json(plan.glass_identity));
        out.emplace("reference_model", json::Value{std::move(reference_model)});
        out.emplace("plan_id", json::Value{plan.plan_id});
        out.emplace(
            "command_descriptor",
            json::Value{std::move(command_descriptor)});
        out.emplace("geometry", json::Value{std::move(geometry)});
        out.emplace("shape", json::Value{std::move(shape)});
        out.emplace("render_target", json::Value{std::move(render_target)});
        out.emplace(
            "capability_snapshot",
            json::Value{std::move(capability_snapshot)});
        out.emplace("decision_trace", json::Value{std::move(decision_trace)});
        out.emplace("opacity", json::Value{plan.opacity});
        out.emplace("blur_radius", json::Value{plan.blur_radius});
        out.emplace("tint", json::Value{std::move(tint)});
        out.emplace("saturation", json::Value{plan.saturation});
        out.emplace("luminance_floor", json::Value{plan.luminance_floor});
        out.emplace("luminance_gain", json::Value{plan.luminance_gain});
        out.emplace(
            "luminance_curve",
            json::Value{std::move(luminance_curve)});
        out.emplace("edge_highlight", json::Value{plan.edge_highlight});
        out.emplace("edge_width", json::Value{plan.edge_width});
        out.emplace("noise_opacity", json::Value{plan.noise_opacity});
        out.emplace("shadow_alpha", json::Value{plan.shadow_alpha});
        out.emplace("shadow_radius", json::Value{plan.shadow_radius});
        out.emplace("backdrop_sampling", json::Value{plan.backdrop_sampling});
        out.emplace("backdrop", json::Value{std::move(backdrop)});
        out.emplace(
            "backdrop_access",
            json::Value{std::move(backdrop_access)});
        out.emplace("theme", json::Value{std::move(theme)});
        out.emplace("foreground", json::Value{std::move(foreground)});
        out.emplace("refraction", json::Value{std::move(refraction)});
        out.emplace("specular", json::Value{std::move(specular)});
        out.emplace("edge_optics", json::Value{std::move(edge_optics)});
        out.emplace("spectral_tint", json::Value{std::move(spectral_tint)});
        out.emplace(
            "dynamic_lighting",
            json::Value{std::move(dynamic_lighting)});
        out.emplace(
            "glass_thickness",
            json::Value{std::move(glass_thickness)});
        out.emplace(
            "glass_dispersion",
            json::Value{std::move(glass_dispersion)});
        out.emplace(
            "scroll_edge",
            material_scroll_edge_profile_json(plan.scroll_edge));
        out.emplace(
            "clear_glass_legibility",
            material_clear_glass_legibility_profile_json(
                plan.clear_glass_legibility));
        out.emplace(
            "prominent_glass",
            json::Value{std::move(prominent_glass)});
        out.emplace(
            "large_surface_legibility",
            json::Value{std::move(large_surface_legibility)});
        out.emplace("interaction", json::Value{std::move(interaction)});
        out.emplace(
            "optical_response",
            json::Value{std::move(optical_response)});
        out.emplace(
            "optical_composition",
            json::Value{std::move(optical_composition)});
        out.emplace("fallback", json::Value{plan.fallback()});
        out.emplace(
            "fallback_path",
            json::Value{material_fallback_path_name(plan.fallback_path)});
        out.emplace("fallback_reason", json::Value{plan.fallback_reason});
        out.emplace("contrast_intent", json::Value{plan.contrast_intent});
        out.emplace(
            "debug_seed",
            json::Value{static_cast<std::int64_t>(plan.debug_seed)});
        out.emplace(
            "sample_taps",
            json::Value{static_cast<std::int64_t>(plan.sample_taps)});
        out.emplace(
            "sampling_kernel",
            json::Value{std::move(sampling_kernel)});
        out.emplace("quality_policy", json::Value{std::move(quality_policy)});
        out.emplace("primary_pass", json::Value{std::move(primary_pass)});
        out.emplace("resource_budget", json::Value{std::move(resource_budget)});
        out.emplace(
            "execution_stage_capacity",
            json::Value{
                static_cast<std::int64_t>(plan.execution_stage_capacity)});
        out.emplace(
            "dropped_execution_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    plan.dropped_execution_stage_count)});
        out.emplace(
            "paint_layer_capacity",
            json::Value{
                static_cast<std::int64_t>(plan.paint_layer_capacity)});
        out.emplace(
            "dropped_paint_layer_count",
            json::Value{
                static_cast<std::int64_t>(
                    plan.dropped_paint_layer_count)});
        out.emplace("verifier", json::Value{std::move(verifier)});
        out.emplace(
            "observation_contract",
            json::Value{std::move(observation_contract)});
        out.emplace(
            "execution_audit",
            json::Value{std::move(execution_audit)});
        out.emplace("passes", json::Value{std::move(passes)});
        out.emplace("execution_stages", json::Value{std::move(execution_stages)});
        out.emplace("paint_layers", json::Value{std::move(paint_layers)});
        return json::Value{std::move(out)};
    }

    inline json::Array material_plans_runtime_json(
            std::vector<MaterialRuntimeRecord> const& records) {
        json::Array plans;
        for (auto const& record : records)
            plans.push_back(material_plan_runtime_json(record));
        return plans;
    }

    inline json::Value material_container_group_member_json(
            MaterialRuntimeRecord const& record,
            MaterialContainerExecutionDescriptor const& execution) {
        auto const& plan = record.plan;
        json::Object geometry;
        geometry.emplace("x", json::Value{plan.geometry.x});
        geometry.emplace("y", json::Value{plan.geometry.y});
        geometry.emplace("w", json::Value{plan.geometry.w});
        geometry.emplace("h", json::Value{plan.geometry.h});
        geometry.emplace("radius", json::Value{plan.geometry.radius});

        json::Object out;
        out.emplace(
            "command_index",
            json::Value{static_cast<std::int64_t>(record.command_index)});
        out.emplace("plan_id", json::Value{plan.plan_id});
        out.emplace("kind", json::Value{material_kind_name(plan.kind)});
        out.emplace(
            "role",
            json::Value{material_surface_role_name(plan.role)});
        out.emplace(
            "fallback_path",
            json::Value{material_fallback_path_name(plan.fallback_path)});
        out.emplace("backdrop_sampling", json::Value{plan.backdrop_sampling});
        out.emplace("active_pass", json::Value{plan.primary_pass.active});
        out.emplace("interactive", json::Value{plan.container.interactive});
        out.emplace(
            "union_id",
            json::Value{static_cast<std::int64_t>(plan.container.union_id)});
        out.emplace(
            "mode",
            json::Value{plan.container.mode_name});
        out.emplace("spacing", json::Value{plan.container.spacing});
        out.emplace(
            "blend_distance",
            json::Value{plan.container.blend_distance});
        out.emplace(
            "shape_union_expected",
            json::Value{plan.container.shape_union_expected});
        out.emplace(
            "shape_blending_expected",
            json::Value{plan.container.shape_blending_expected});
        out.emplace(
            "morph_transitions",
            json::Value{plan.container.morph_transitions});
        out.emplace(
            "glass_namespace_id",
            json::Value{
                static_cast<std::int64_t>(
                    plan.glass_identity.namespace_id)});
        out.emplace(
            "glass_effect_id",
            json::Value{
                static_cast<std::int64_t>(
                    plan.glass_identity.effect_id)});
        out.emplace(
            "glass_effect_participates",
            json::Value{plan.glass_identity.participates});
        out.emplace(
            "glass_effect_matched_geometry_candidate",
            json::Value{plan.glass_identity.matched_geometry_candidate});
        out.emplace(
            "glass_effect_match_execution",
            json::Value{execution.glass_effect_match_execution});
        out.emplace(
            "glass_effect_surface_count",
            json::Value{
                static_cast<std::int64_t>(
                    execution.glass_effect_surface_count)});
        out.emplace(
            "glass_effect_match_progress",
            json::Value{execution.glass_effect_match_progress});
        out.emplace(
            "glass_effect_match_blend_strength",
            json::Value{execution.glass_effect_match_blend_strength});
        out.emplace(
            "glass_effect_materialize_execution",
            json::Value{execution.glass_effect_materialize_execution});
        out.emplace(
            "glass_effect_materialize_progress",
            json::Value{execution.glass_effect_materialize_progress});
        out.emplace(
            "glass_effect_materialize_wave_strength",
            json::Value{execution.glass_effect_materialize_wave_strength});
        json::Object match_source;
        match_source.emplace(
            "valid",
            json::Value{execution.glass_effect_match_source_valid});
        match_source.emplace(
            "x",
            json::Value{execution.glass_effect_match_source_x});
        match_source.emplace(
            "y",
            json::Value{execution.glass_effect_match_source_y});
        match_source.emplace(
            "w",
            json::Value{execution.glass_effect_match_source_w});
        match_source.emplace(
            "h",
            json::Value{execution.glass_effect_match_source_h});
        match_source.emplace(
            "radius",
            json::Value{execution.glass_effect_match_source_radius});
        match_source.emplace(
            "gap",
            json::Value{execution.glass_effect_match_source_gap});
        match_source.emplace(
            "spacing",
            json::Value{execution.glass_effect_match_source_spacing});
        match_source.emplace(
            "proximity",
            json::Value{execution.glass_effect_match_source_proximity});
        match_source.emplace(
            "effect_id_match",
            json::Value{
                execution.glass_effect_match_source_effect_id_match});
        out.emplace(
            "glass_effect_match_source",
            json::Value{std::move(match_source)});
        json::Object match_rect;
        match_rect.emplace(
            "valid",
            json::Value{execution.glass_effect_match_source_valid});
        match_rect.emplace(
            "x",
            json::Value{execution.glass_effect_match_rect_x});
        match_rect.emplace(
            "y",
            json::Value{execution.glass_effect_match_rect_y});
        match_rect.emplace(
            "w",
            json::Value{execution.glass_effect_match_rect_w});
        match_rect.emplace(
            "h",
            json::Value{execution.glass_effect_match_rect_h});
        match_rect.emplace(
            "radius",
            json::Value{execution.glass_effect_match_rect_radius});
        out.emplace(
            "glass_effect_match_rect",
            json::Value{std::move(match_rect)});
        json::Object match_appearance;
        match_appearance.emplace(
            "active",
            json::Value{execution.glass_effect_match_appearance_active});
        match_appearance.emplace(
            "source_command_index",
            json::Value{
                static_cast<std::int64_t>(
                    execution
                        .glass_effect_match_appearance_source_command_index)});
        match_appearance.emplace(
            "blend",
            json::Value{execution.glass_effect_match_appearance_blend});
        match_appearance.emplace(
            "tint_active",
            json::Value{
                execution.glass_effect_match_appearance_tint_active});
        match_appearance.emplace(
            "tint_r",
            json::Value{execution.glass_effect_match_appearance_tint_r});
        match_appearance.emplace(
            "tint_g",
            json::Value{execution.glass_effect_match_appearance_tint_g});
        match_appearance.emplace(
            "tint_b",
            json::Value{execution.glass_effect_match_appearance_tint_b});
        match_appearance.emplace(
            "tint_a",
            json::Value{execution.glass_effect_match_appearance_tint_a});
        match_appearance.emplace(
            "spectral_tint_active",
            json::Value{
                execution
                    .glass_effect_match_appearance_spectral_tint_active});
        match_appearance.emplace(
            "spectral_tint_warmth",
            json::Value{
                execution
                    .glass_effect_match_appearance_spectral_tint_warmth});
        match_appearance.emplace(
            "spectral_tint_coolness",
            json::Value{
                execution
                    .glass_effect_match_appearance_spectral_tint_coolness});
        match_appearance.emplace(
            "spectral_tint_dispersion",
            json::Value{
                execution
                    .glass_effect_match_appearance_spectral_tint_dispersion});
        match_appearance.emplace(
            "spectral_tint_rim",
            json::Value{
                execution.glass_effect_match_appearance_spectral_tint_rim});
        match_appearance.emplace(
            "prominent_glass_active",
            json::Value{
                execution
                    .glass_effect_match_appearance_prominent_glass_active});
        match_appearance.emplace(
            "prominent_glass_intensity",
            json::Value{
                execution
                    .glass_effect_match_appearance_prominent_glass_intensity});
        match_appearance.emplace(
            "prominent_glass_tint_weight",
            json::Value{
                execution
                    .glass_effect_match_appearance_prominent_glass_tint_weight});
        match_appearance.emplace(
            "prominent_glass_edge_lift",
            json::Value{
                execution
                    .glass_effect_match_appearance_prominent_glass_edge_lift});
        match_appearance.emplace(
            "prominent_glass_lensing_gain",
            json::Value{
                execution
                    .glass_effect_match_appearance_prominent_glass_lensing_gain});
        match_appearance.emplace(
            "clear_glass_active",
            json::Value{
                execution.glass_effect_match_appearance_clear_glass_active});
        match_appearance.emplace(
            "clear_glass_dimming",
            json::Value{
                execution.glass_effect_match_appearance_clear_glass_dimming});
        match_appearance.emplace(
            "clear_glass_contrast",
            json::Value{
                execution.glass_effect_match_appearance_clear_glass_contrast});
        match_appearance.emplace(
            "clear_glass_brightness_response",
            json::Value{
                execution
                    .glass_effect_match_appearance_clear_glass_brightness_response});
        match_appearance.emplace(
            "clear_glass_detail_response",
            json::Value{
                execution
                    .glass_effect_match_appearance_clear_glass_detail_response});
        out.emplace(
            "glass_effect_match_appearance",
            json::Value{std::move(match_appearance)});
        out.emplace(
            "shared_backdrop_scope",
            json::Value{plan.container.shared_backdrop_scope});
        out.emplace(
            "group_execution_policy",
            json::Value{execution.execution_policy});
        json::Object group_overlap_response;
        group_overlap_response.emplace(
            "active",
            json::Value{execution.overlap_response_active});
        group_overlap_response.emplace(
            "pair_count",
            json::Value{
                static_cast<std::int64_t>(execution.overlap_pair_count)});
        group_overlap_response.emplace(
            "strength",
            json::Value{execution.overlap_response_strength});
        group_overlap_response.emplace(
            "max_fraction",
            json::Value{execution.overlap_max_fraction});
        group_overlap_response.emplace(
            "density",
            json::Value{execution.overlap_density});
        out.emplace(
            "group_overlap_response",
            json::Value{std::move(group_overlap_response)});
        json::Object group_interaction_source;
        group_interaction_source.emplace(
            "valid",
            json::Value{execution.group_interaction_source_valid});
        group_interaction_source.emplace(
            "command_index",
            json::Value{
                static_cast<std::int64_t>(
                    execution.group_interaction_source_command_index)});
        group_interaction_source.emplace(
            "specular_anchor_x",
            json::Value{execution.group_interaction_specular_anchor_x});
        group_interaction_source.emplace(
            "specular_anchor_y",
            json::Value{execution.group_interaction_specular_anchor_y});
        group_interaction_source.emplace(
            "specular_radius",
            json::Value{execution.group_interaction_specular_radius});
        group_interaction_source.emplace(
            "specular_intensity",
            json::Value{execution.group_interaction_specular_intensity});
        group_interaction_source.emplace(
            "pointer_lens_active",
            json::Value{execution.group_interaction_pointer_lens_active});
        group_interaction_source.emplace(
            "pointer_lens_anchor_x",
            json::Value{execution.group_interaction_pointer_lens_anchor_x});
        group_interaction_source.emplace(
            "pointer_lens_anchor_y",
            json::Value{execution.group_interaction_pointer_lens_anchor_y});
        group_interaction_source.emplace(
            "pointer_lens_radius",
            json::Value{execution.group_interaction_pointer_lens_radius});
        group_interaction_source.emplace(
            "pointer_lens_strength",
            json::Value{execution.group_interaction_pointer_lens_strength});
        group_interaction_source.emplace(
            "control_morph_active",
            json::Value{execution.group_interaction_control_morph_active});
        group_interaction_source.emplace(
            "control_morph_scale_delta",
            json::Value{
                execution.group_interaction_control_morph_scale_delta});
        group_interaction_source.emplace(
            "control_morph_depth",
            json::Value{execution.group_interaction_control_morph_depth});
        group_interaction_source.emplace(
            "control_morph_edge",
            json::Value{execution.group_interaction_control_morph_edge});
        group_interaction_source.emplace(
            "control_morph_shadow",
            json::Value{execution.group_interaction_control_morph_shadow});
        group_interaction_source.emplace(
            "refraction_active",
            json::Value{execution.group_interaction_refraction_active});
        group_interaction_source.emplace(
            "refraction_strength",
            json::Value{execution.group_interaction_refraction_strength});
        group_interaction_source.emplace(
            "refraction_edge_bias",
            json::Value{execution.group_interaction_refraction_edge_bias});
        group_interaction_source.emplace(
            "refraction_max_offset_pixels",
            json::Value{
                execution.group_interaction_refraction_max_offset_pixels});
        group_interaction_source.emplace(
            "refraction_edge_caustic_intensity",
            json::Value{
                execution
                    .group_interaction_refraction_edge_caustic_intensity});
        group_interaction_source.emplace(
            "dynamic_lighting_active",
            json::Value{
                execution.group_interaction_dynamic_lighting_active});
        group_interaction_source.emplace(
            "dynamic_light_direction_x",
            json::Value{
                execution.group_interaction_dynamic_light_direction_x});
        group_interaction_source.emplace(
            "dynamic_light_direction_y",
            json::Value{
                execution.group_interaction_dynamic_light_direction_y});
        group_interaction_source.emplace(
            "dynamic_light_highlight",
            json::Value{
                execution.group_interaction_dynamic_light_highlight});
        group_interaction_source.emplace(
            "dynamic_light_shadow",
            json::Value{execution.group_interaction_dynamic_light_shadow});
        group_interaction_source.emplace(
            "glass_thickness_active",
            json::Value{
                execution.group_interaction_glass_thickness_active});
        group_interaction_source.emplace(
            "glass_thickness",
            json::Value{execution.group_interaction_glass_thickness});
        group_interaction_source.emplace(
            "glass_lensing_gain",
            json::Value{execution.group_interaction_glass_lensing_gain});
        group_interaction_source.emplace(
            "glass_shadow_gain",
            json::Value{execution.group_interaction_glass_shadow_gain});
        group_interaction_source.emplace(
            "glass_scattering_gain",
            json::Value{execution.group_interaction_glass_scattering_gain});
        group_interaction_source.emplace(
            "glass_dispersion_active",
            json::Value{
                execution.group_interaction_glass_dispersion_active});
        group_interaction_source.emplace(
            "glass_dispersion_axial_offset",
            json::Value{
                execution
                    .group_interaction_glass_dispersion_axial_offset});
        group_interaction_source.emplace(
            "glass_dispersion_tangential_offset",
            json::Value{
                execution
                    .group_interaction_glass_dispersion_tangential_offset});
        group_interaction_source.emplace(
            "glass_dispersion_prismatic_gain",
            json::Value{
                execution
                    .group_interaction_glass_dispersion_prismatic_gain});
        group_interaction_source.emplace(
            "glass_dispersion_caustic_spread",
            json::Value{
                execution
                    .group_interaction_glass_dispersion_caustic_spread});
        out.emplace(
            "group_interaction_source",
            json::Value{std::move(group_interaction_source)});
        json::Object group_appearance_source;
        group_appearance_source.emplace(
            "valid",
            json::Value{execution.group_appearance_source_valid});
        group_appearance_source.emplace(
            "command_index",
            json::Value{
                static_cast<std::int64_t>(
                    execution.group_appearance_source_command_index)});
        group_appearance_source.emplace(
            "tint_active",
            json::Value{execution.group_appearance_tint_active});
        group_appearance_source.emplace(
            "tint_r",
            json::Value{execution.group_appearance_tint_r});
        group_appearance_source.emplace(
            "tint_g",
            json::Value{execution.group_appearance_tint_g});
        group_appearance_source.emplace(
            "tint_b",
            json::Value{execution.group_appearance_tint_b});
        group_appearance_source.emplace(
            "tint_a",
            json::Value{execution.group_appearance_tint_a});
        group_appearance_source.emplace(
            "spectral_tint_active",
            json::Value{
                execution.group_appearance_spectral_tint_active});
        group_appearance_source.emplace(
            "spectral_tint_warmth",
            json::Value{
                execution.group_appearance_spectral_tint_warmth});
        group_appearance_source.emplace(
            "spectral_tint_coolness",
            json::Value{
                execution.group_appearance_spectral_tint_coolness});
        group_appearance_source.emplace(
            "spectral_tint_dispersion",
            json::Value{
                execution.group_appearance_spectral_tint_dispersion});
        group_appearance_source.emplace(
            "spectral_tint_rim",
            json::Value{execution.group_appearance_spectral_tint_rim});
        group_appearance_source.emplace(
            "prominent_glass_active",
            json::Value{
                execution.group_appearance_prominent_glass_active});
        group_appearance_source.emplace(
            "prominent_glass_intensity",
            json::Value{
                execution.group_appearance_prominent_glass_intensity});
        group_appearance_source.emplace(
            "prominent_glass_tint_weight",
            json::Value{
                execution.group_appearance_prominent_glass_tint_weight});
        group_appearance_source.emplace(
            "prominent_glass_edge_lift",
            json::Value{
                execution.group_appearance_prominent_glass_edge_lift});
        group_appearance_source.emplace(
            "prominent_glass_lensing_gain",
            json::Value{
                execution.group_appearance_prominent_glass_lensing_gain});
        group_appearance_source.emplace(
            "clear_glass_active",
            json::Value{execution.group_appearance_clear_glass_active});
        group_appearance_source.emplace(
            "clear_glass_dimming",
            json::Value{execution.group_appearance_clear_glass_dimming});
        group_appearance_source.emplace(
            "clear_glass_contrast",
            json::Value{execution.group_appearance_clear_glass_contrast});
        group_appearance_source.emplace(
            "clear_glass_brightness_response",
            json::Value{
                execution
                    .group_appearance_clear_glass_brightness_response});
        group_appearance_source.emplace(
            "clear_glass_detail_response",
            json::Value{
                execution.group_appearance_clear_glass_detail_response});
        out.emplace(
            "group_appearance_source",
            json::Value{std::move(group_appearance_source)});
        out.emplace("group_radius", json::Value{execution.group_radius});
        out.emplace(
            "fusion_model",
            json::Value{execution.fusion_model});
        out.emplace(
            "fusion_optics_active",
            json::Value{execution.fusion_optics_active});
        out.emplace(
            "shape_blend_execution",
            json::Value{execution.shape_blend_execution});
        out.emplace(
            "shape_blend_strength",
            json::Value{execution.shape_blend_strength});
        out.emplace(
            "inner_edge_alpha_blend_strength",
            json::Value{execution.inner_edge_alpha_blend_strength});
        out.emplace(
            "fusion_strength",
            json::Value{execution.fusion_strength});
        out.emplace(
            "fusion_lensing_gain",
            json::Value{execution.fusion_lensing_gain});
        out.emplace(
            "fusion_edge_lift",
            json::Value{execution.fusion_edge_lift});
        out.emplace(
            "fusion_shadow_gain",
            json::Value{execution.fusion_shadow_gain});
        out.emplace(
            "reduced_motion_suppressed_morph",
            json::Value{plan.container.reduced_motion_suppressed_morph});
        out.emplace(
            "shape_kind",
            json::Value{material_shape_kind_name(plan.shape.kind)});
        out.emplace("shape_valid", json::Value{plan.shape.valid});
        out.emplace("geometry", json::Value{std::move(geometry)});
        return json::Value{std::move(out)};
    }

    inline json::Value material_container_group_detail_json(
            MaterialContainerGroupAccumulator const& group,
            std::span<MaterialRuntimeRecord const> records,
            json::Array members) {
        auto const bounds_width = group.has_bounds
            ? std::max(0.0f, group.max_x - group.min_x)
            : 0.0f;
        auto const bounds_height = group.has_bounds
            ? std::max(0.0f, group.max_y - group.min_y)
            : 0.0f;
        auto const shape_blend_execution =
            material_container_group_shape_blend_execution_active(group);
        auto const shape_blend_strength =
            material_container_group_shape_blend_strength(group);
        auto const inner_edge_alpha_blend_strength =
            material_container_group_inner_edge_alpha_blend_strength(group);

        json::Object bounds;
        bounds.emplace("valid", json::Value{group.has_bounds});
        bounds.emplace("x", json::Value{group.has_bounds ? group.min_x : 0.0f});
        bounds.emplace("y", json::Value{group.has_bounds ? group.min_y : 0.0f});
        bounds.emplace("w", json::Value{bounds_width});
        bounds.emplace("h", json::Value{bounds_height});
        bounds.emplace(
            "radius",
            json::Value{material_container_group_execution_radius(group)});
        bounds.emplace("area", json::Value{bounds_width * bounds_height});

        json::Object out;
        out.emplace(
            "container_id",
            json::Value{static_cast<std::int64_t>(group.container_id)});
        out.emplace(
            "surface_count",
            json::Value{static_cast<std::int64_t>(group.surface_count)});
        out.emplace(
            "active_surfaces",
            json::Value{static_cast<std::int64_t>(group.active_surfaces)});
        out.emplace(
            "sampled_backdrop_surfaces",
            json::Value{
                static_cast<std::int64_t>(group.sampled_backdrop_surfaces)});
        out.emplace(
            "fallback_surfaces",
            json::Value{static_cast<std::int64_t>(group.fallback_surfaces)});
        out.emplace(
            "union_surfaces",
            json::Value{static_cast<std::int64_t>(group.union_surfaces)});
        out.emplace(
            "morph_surfaces",
            json::Value{static_cast<std::int64_t>(group.morph_surfaces)});
        out.emplace(
            "interactive_surfaces",
            json::Value{
                static_cast<std::int64_t>(group.interactive_surfaces)});
        out.emplace(
            "shared_backdrop_scope_surfaces",
            json::Value{
                static_cast<std::int64_t>(
                    group.shared_backdrop_scope_surfaces)});
        out.emplace(
            "shared_capture_saved_surfaces",
            json::Value{
                static_cast<std::int64_t>(
                    group.shared_backdrop_scope_surfaces > 1u
                        ? group.shared_backdrop_scope_surfaces - 1u
                        : 0u)});
        out.emplace(
            "execution_policy",
            json::Value{material_container_execution_policy_name(group)});
        out.emplace(
            "shape_blend_execution",
            json::Value{shape_blend_execution});
        out.emplace(
            "shape_blend_execution_surfaces",
            json::Value{
                static_cast<std::int64_t>(
                    material_container_group_shape_blend_surface_count(
                        records,
                        group))});
        out.emplace(
            "shape_blend_strength",
            json::Value{shape_blend_strength});
        out.emplace(
            "inner_edge_alpha_blend_strength",
            json::Value{inner_edge_alpha_blend_strength});
        out.emplace(
            "shape_pair_count",
            json::Value{static_cast<std::int64_t>(group.shape_pair_count)});
        out.emplace(
            "blend_candidate_pair_count",
            json::Value{
                static_cast<std::int64_t>(
                    group.blend_candidate_pair_count)});
        out.emplace(
            "union_candidate_pair_count",
            json::Value{
                static_cast<std::int64_t>(
                    group.union_candidate_pair_count)});
        out.emplace(
            "morph_candidate_pair_count",
            json::Value{
                static_cast<std::int64_t>(
                    group.morph_candidate_pair_count)});
        out.emplace(
            "separated_pair_count",
            json::Value{static_cast<std::int64_t>(group.separated_pair_count)});
        out.emplace(
            "min_shape_gap",
            json::Value{group.has_shape_gap ? group.min_shape_gap : 0.0f});
        out.emplace(
            "max_shape_gap",
            json::Value{group.has_shape_gap ? group.max_shape_gap : 0.0f});
        out.emplace(
            "max_blend_distance",
            json::Value{group.max_blend_distance});
        out.emplace("bounds", json::Value{std::move(bounds)});
        out.emplace(
            "likely_layer",
            json::Value{"material-container"});
        out.emplace(
            "likely_pass",
            json::Value{"container-group-analysis"});
        out.emplace(
            "members",
            json::Value{std::move(members)});
        return json::Value{std::move(out)};
    }

    inline json::Array material_container_group_details_json(
            std::vector<MaterialRuntimeRecord> const& records) {
        json::Array groups;
        for (std::size_t index = 0; index < records.size(); ++index) {
            auto const& plan = records[index].plan;
            if (!plan.container.participates || plan.container.container_id == 0u)
                continue;
            auto const container_id = plan.container.container_id;
            auto seen = false;
            for (std::size_t prior = 0; prior < index; ++prior) {
                auto const& prior_plan = records[prior].plan;
                if (prior_plan.container.participates
                    && prior_plan.container.container_id == container_id) {
                    seen = true;
                    break;
                }
            }
            if (seen)
                continue;

            MaterialContainerGroupAccumulator group{
                .container_id = container_id,
            };
            json::Array members;
            for (auto const& candidate_record : records) {
                auto const& candidate = candidate_record.plan;
                if (!candidate.container.participates
                    || candidate.container.container_id != container_id) {
                    continue;
                }
                ++group.surface_count;
                accumulate_material_container_bounds(group, candidate);
                if (candidate.primary_pass.active)
                    ++group.active_surfaces;
                if (candidate.backdrop_sampling)
                    ++group.sampled_backdrop_surfaces;
                if (candidate.fallback())
                    ++group.fallback_surfaces;
                if (candidate.container.shape_union_expected)
                    ++group.union_surfaces;
                if (candidate.container.morph_transitions)
                    ++group.morph_surfaces;
                if (candidate.container.interactive)
                    ++group.interactive_surfaces;
                if (candidate.container.shared_backdrop_scope)
                    ++group.shared_backdrop_scope_surfaces;
                auto const execution =
                    material_container_execution_descriptor(
                        candidate_record,
                        records);
                members.push_back(
                    material_container_group_member_json(
                        candidate_record,
                        execution));
            }
            for (std::size_t left = 0; left < records.size(); ++left) {
                auto const& a = records[left].plan;
                if (!material_plan_in_container(a, container_id))
                    continue;
                for (std::size_t right = left + 1;
                     right < records.size();
                     ++right) {
                    auto const& b = records[right].plan;
                    if (!material_plan_in_container(b, container_id))
                        continue;
                    accumulate_material_container_pair(group, a, b);
                }
            }
            groups.push_back(
                material_container_group_detail_json(
                    group,
                    records,
                    std::move(members)));
        }
        return groups;
    }

    inline json::Value material_container_group_summary_json(
            MaterialContainerGroupRuntimeSummary const& summary) {
        json::Object out;
        out.emplace(
            "group_count",
            json::Value{static_cast<std::int64_t>(summary.group_count)});
        out.emplace(
            "multi_surface_group_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.multi_surface_group_count)});
        out.emplace(
            "union_group_count",
            json::Value{static_cast<std::int64_t>(summary.union_group_count)});
        out.emplace(
            "morph_group_count",
            json::Value{static_cast<std::int64_t>(summary.morph_group_count)});
        out.emplace(
            "interactive_group_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interactive_group_count)});
        out.emplace(
            "shared_backdrop_scope_group_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.shared_backdrop_scope_group_count)});
        out.emplace(
            "shared_capture_surface_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.shared_capture_surface_count)});
        out.emplace(
            "shared_capture_saved_surface_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.shared_capture_saved_surface_count)});
        out.emplace(
            "max_shared_capture_group_surfaces",
            json::Value{
                static_cast<std::int64_t>(
                    summary.max_shared_capture_group_surfaces)});
        out.emplace(
            "shape_blend_execution_group_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.shape_blend_execution_group_count)});
        out.emplace(
            "shape_blend_execution_surface_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.shape_blend_execution_surface_count)});
        out.emplace(
            "fallback_mixed_group_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.fallback_mixed_group_count)});
        out.emplace(
            "max_group_size",
            json::Value{static_cast<std::int64_t>(summary.max_group_size)});
        out.emplace(
            "max_active_surfaces",
            json::Value{
                static_cast<std::int64_t>(summary.max_active_surfaces)});
        out.emplace(
            "max_sampled_backdrop_surfaces",
            json::Value{
                static_cast<std::int64_t>(
                    summary.max_sampled_backdrop_surfaces)});
        out.emplace(
            "max_fallback_surfaces",
            json::Value{
                static_cast<std::int64_t>(summary.max_fallback_surfaces)});
        out.emplace(
            "total_shape_pair_count",
            json::Value{
                static_cast<std::int64_t>(summary.total_shape_pair_count)});
        out.emplace(
            "blend_candidate_pair_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.blend_candidate_pair_count)});
        out.emplace(
            "union_candidate_pair_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.union_candidate_pair_count)});
        out.emplace(
            "morph_candidate_pair_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.morph_candidate_pair_count)});
        out.emplace(
            "separated_pair_count",
            json::Value{
                static_cast<std::int64_t>(summary.separated_pair_count)});
        out.emplace("min_shape_gap", json::Value{summary.min_shape_gap});
        out.emplace("max_shape_gap", json::Value{summary.max_shape_gap});
        out.emplace(
            "max_blend_distance",
            json::Value{summary.max_blend_distance});
        out.emplace(
            "max_group_bounds_width",
            json::Value{summary.max_group_bounds_width});
        out.emplace(
            "max_group_bounds_height",
            json::Value{summary.max_group_bounds_height});
        out.emplace(
            "max_group_bounds_area",
            json::Value{summary.max_group_bounds_area});
        out.emplace(
            "max_shape_blend_strength",
            json::Value{summary.max_shape_blend_strength});
        return json::Value{std::move(out)};
    }

    inline json::Value material_runtime_summary_json(
            MaterialRuntimeSummary const& summary) {
        json::Object out;
        out.emplace(
            "plan_count",
            json::Value{static_cast<std::int64_t>(summary.plan_count)});
        out.emplace(
            "fallback_count",
            json::Value{static_cast<std::int64_t>(summary.fallback_count)});
        out.emplace(
            "backdrop_sampling_count",
            json::Value{
                static_cast<std::int64_t>(summary.backdrop_sampling_count)});
        out.emplace(
            "total_runtime_passes",
            json::Value{
                static_cast<std::int64_t>(summary.total_runtime_passes)});
        out.emplace(
            "active_runtime_passes",
            json::Value{
                static_cast<std::int64_t>(summary.active_runtime_passes)});
        out.emplace(
            "backdrop_runtime_passes",
            json::Value{
                static_cast<std::int64_t>(summary.backdrop_runtime_passes)});
        out.emplace(
            "total_execution_stages",
            json::Value{
                static_cast<std::int64_t>(summary.total_execution_stages)});
        out.emplace(
            "active_execution_stages",
            json::Value{
                static_cast<std::int64_t>(summary.active_execution_stages)});
        out.emplace(
            "backdrop_execution_stages",
            json::Value{
                static_cast<std::int64_t>(summary.backdrop_execution_stages)});
        out.emplace(
            "dropped_execution_stages",
            json::Value{
                static_cast<std::int64_t>(summary.dropped_execution_stages)});
        out.emplace(
            "max_execution_stage_count",
            json::Value{
                static_cast<std::int64_t>(summary.max_execution_stage_count)});
        out.emplace(
            "max_execution_stages",
            json::Value{
                static_cast<std::int64_t>(summary.max_execution_stages)});
        out.emplace(
            "max_execution_stage_capacity",
            json::Value{
                static_cast<std::int64_t>(
                    summary.max_execution_stage_capacity)});
        out.emplace(
            "total_paint_layers",
            json::Value{
                static_cast<std::int64_t>(summary.total_paint_layers)});
        out.emplace(
            "active_paint_layers",
            json::Value{
                static_cast<std::int64_t>(summary.active_paint_layers)});
        out.emplace(
            "dropped_paint_layers",
            json::Value{
                static_cast<std::int64_t>(summary.dropped_paint_layers)});
        out.emplace(
            "shadow_paint_layers",
            json::Value{
                static_cast<std::int64_t>(summary.shadow_paint_layers)});
        out.emplace(
            "fill_paint_layers",
            json::Value{
                static_cast<std::int64_t>(summary.fill_paint_layers)});
        out.emplace(
            "edge_paint_layers",
            json::Value{
                static_cast<std::int64_t>(summary.edge_paint_layers)});
        out.emplace(
            "max_paint_layer_count",
            json::Value{
                static_cast<std::int64_t>(summary.max_paint_layer_count)});
        out.emplace(
            "max_paint_layers",
            json::Value{
                static_cast<std::int64_t>(summary.max_paint_layers)});
        out.emplace(
            "max_paint_layer_capacity",
            json::Value{
                static_cast<std::int64_t>(summary.max_paint_layer_capacity)});
        out.emplace(
            "max_paint_layer_inflate",
            json::Value{summary.max_paint_layer_inflate});
        out.emplace(
            "max_pass_texture_copy_pixels",
            json::Value{summary.max_pass_texture_copy_pixels});
        out.emplace(
            "total_pass_texture_copy_pixels",
            json::Value{summary.total_pass_texture_copy_pixels});
        out.emplace(
            "backdrop_access_count",
            json::Value{
                static_cast<std::int64_t>(summary.backdrop_access_count)});
        out.emplace(
            "shared_frame_capture_plan_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.shared_frame_capture_plan_count)});
        out.emplace(
            "next_frame_capture_plan_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.next_frame_capture_plan_count)});
        out.emplace(
            "max_frame_capture_count",
            json::Value{
                static_cast<std::int64_t>(summary.max_frame_capture_count)});
        out.emplace(
            "max_frame_capture_pixels",
            json::Value{summary.max_frame_capture_pixels});
        out.emplace(
            "total_surface_sample_pixels",
            json::Value{summary.total_surface_sample_pixels});
        out.emplace(
            "max_surface_sample_pixels",
            json::Value{summary.max_surface_sample_pixels});
        out.emplace("max_plan_blur_radius",
                    json::Value{summary.max_plan_blur_radius});
        out.emplace(
            "max_plan_sample_taps",
            json::Value{
                static_cast<std::int64_t>(summary.max_plan_sample_taps)});
        out.emplace(
            "total_plan_sample_taps",
            json::Value{summary.total_plan_sample_taps});
        out.emplace("max_budget_blur_radius",
                    json::Value{summary.max_budget_blur_radius});
        out.emplace(
            "max_sample_taps",
            json::Value{
                static_cast<std::int64_t>(summary.max_sample_taps)});
        out.emplace(
            "max_sampling_kernel_radius",
            json::Value{
                static_cast<std::int64_t>(
                    summary.max_sampling_kernel_radius)});
        out.emplace(
            "max_pass_count",
            json::Value{static_cast<std::int64_t>(summary.max_pass_count)});
        out.emplace("max_backdrop_pixels",
                    json::Value{summary.max_backdrop_pixels});
        out.emplace("max_container_spacing",
                    json::Value{summary.max_container_spacing});
        out.emplace(
            "containered_count",
            json::Value{
                static_cast<std::int64_t>(summary.containered_count)});
        out.emplace(
            "unioned_count",
            json::Value{
                static_cast<std::int64_t>(summary.unioned_count)});
        out.emplace(
            "interactive_count",
            json::Value{
                static_cast<std::int64_t>(summary.interactive_count)});
        out.emplace(
            "morph_transition_count",
            json::Value{
                static_cast<std::int64_t>(summary.morph_transition_count)});
        out.emplace(
            "valid_shape_count",
            json::Value{
                static_cast<std::int64_t>(summary.valid_shape_count)});
        out.emplace(
            "rounded_shape_count",
            json::Value{
                static_cast<std::int64_t>(summary.rounded_shape_count)});
        out.emplace(
            "capsule_shape_count",
            json::Value{
                static_cast<std::int64_t>(summary.capsule_shape_count)});
        out.emplace(
            "radius_clamped_count",
            json::Value{
                static_cast<std::int64_t>(summary.radius_clamped_count)});
        out.emplace(
            "foreground_backdrop_driven_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.foreground_backdrop_driven_count)});
        out.emplace(
            "foreground_high_contrast_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.foreground_high_contrast_count)});
        out.emplace(
            "foreground_vibrant_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.foreground_vibrant_count)});
        out.emplace(
            "interaction_enabled_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_enabled_count)});
        out.emplace(
            "interaction_active_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_active_count)});
        out.emplace(
            "interaction_reduce_motion_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_reduce_motion_count)});
        out.emplace(
            "interaction_specular_highlight_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_specular_highlight_count)});
        out.emplace(
            "interaction_pointer_lens_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_pointer_lens_count)});
        out.emplace(
            "refraction_active_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.refraction_active_count)});
        out.emplace(
            "theme_default_glass_token_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.theme_default_glass_token_count)});
        out.emplace(
            "theme_custom_token_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.theme_custom_token_count)});
        out.emplace(
            "container_groups",
            material_container_group_summary_json(summary.container_groups));
        out.emplace("max_surface_area",
                    json::Value{summary.max_surface_area});
        out.emplace("max_effective_radius",
                    json::Value{summary.max_effective_radius});
        out.emplace("max_radius_limit",
                    json::Value{summary.max_radius_limit});
        out.emplace("max_normalized_radius",
                    json::Value{summary.max_normalized_radius});
        out.emplace("max_saturation",
                    json::Value{summary.max_saturation});
        out.emplace("max_edge_highlight",
                    json::Value{summary.max_edge_highlight});
        out.emplace("max_edge_width",
                    json::Value{summary.max_edge_width});
        out.emplace("max_noise_opacity",
                    json::Value{summary.max_noise_opacity});
        out.emplace("max_shadow_alpha",
                    json::Value{summary.max_shadow_alpha});
        out.emplace("max_shadow_radius",
                    json::Value{summary.max_shadow_radius});
        out.emplace(
            "max_interaction_response_strength",
            json::Value{summary.max_interaction_response_strength});
        out.emplace(
            "max_interaction_specular_radius",
            json::Value{summary.max_interaction_specular_radius});
        out.emplace(
            "max_interaction_specular_intensity",
            json::Value{summary.max_interaction_specular_intensity});
        out.emplace(
            "max_interaction_pointer_lens_radius",
            json::Value{summary.max_interaction_pointer_lens_radius});
        out.emplace(
            "max_interaction_pointer_lens_strength",
            json::Value{summary.max_interaction_pointer_lens_strength});
        out.emplace(
            "max_refraction_strength",
            json::Value{summary.max_refraction_strength});
        out.emplace(
            "max_refraction_edge_bias",
            json::Value{summary.max_refraction_edge_bias});
        out.emplace(
            "max_refraction_offset_pixels",
            json::Value{summary.max_refraction_offset_pixels});
        out.emplace(
            "min_foreground_contrast_ratio",
            json::Value{summary.min_foreground_contrast_ratio});
        out.emplace(
            "unbounded_texture_copy",
            json::Value{
                static_cast<std::int64_t>(
                    summary.unbounded_texture_copy)});
        out.emplace(
            "non_deterministic_fallback",
            json::Value{
                static_cast<std::int64_t>(
                    summary.non_deterministic_fallback)});
        out.emplace(
            "execution_contract_satisfied_count",
            json::Value{static_cast<std::int64_t>(
                summary.execution_contract_satisfied_count)});
        out.emplace(
            "execution_contract_mismatch_count",
            json::Value{static_cast<std::int64_t>(
                summary.execution_contract_mismatch_count)});
        out.emplace(
            "execution_contract_mismatch_total",
            json::Value{static_cast<std::int64_t>(
                summary.execution_contract_mismatch_total)});
        out.emplace(
            "stage_order_match_count",
            json::Value{static_cast<std::int64_t>(
                summary.stage_order_match_count)});
        out.emplace(
            "stage_order_mismatch_count",
            json::Value{static_cast<std::int64_t>(
                summary.stage_order_mismatch_count)});
        out.emplace(
            "first_execution_contract_mismatch",
            json::Value{summary.first_execution_contract_mismatch
                            ? summary.first_execution_contract_mismatch
                            : "none"});
        out.emplace(
            "first_stage_order_mismatch",
            json::Value{summary.first_stage_order_mismatch
                            ? summary.first_stage_order_mismatch
                            : "none"});
        return json::Value{std::move(out)};
    }

    inline json::Value material_runtime_summary_json(
            std::vector<MaterialRuntimeRecord> const& records) {
        MaterialRuntimeSummary summary;
        for (auto const& record : records)
            accumulate_material_runtime_summary(summary, record);
        finalize_material_runtime_summary(summary, records);
        return material_runtime_summary_json(summary);
    }

    inline json::Value material_executor_summary_json(
            MaterialExecutorSummary const& summary) {
        json::Object out;
        out.emplace(
            "plan_count",
            json::Value{static_cast<std::int64_t>(summary.plan_count)});
        out.emplace(
            "material_instance_count",
            json::Value{
                static_cast<std::int64_t>(summary.material_instance_count)});
        out.emplace(
            "fallback_instance_count",
            json::Value{
                static_cast<std::int64_t>(summary.fallback_instance_count)});
        out.emplace(
            "sampled_backdrop_instance_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.sampled_backdrop_instance_count)});
        out.emplace(
            "standard_fill_instance_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.standard_fill_instance_count)});
        out.emplace(
            "deterministic_fallback_instance_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.deterministic_fallback_instance_count)});
        out.emplace(
            "material_draw_calls",
            json::Value{
                static_cast<std::int64_t>(summary.material_draw_calls)});
        out.emplace(
            "backdrop_copy_count",
            json::Value{
                static_cast<std::int64_t>(summary.backdrop_copy_count)});
        out.emplace(
            "backdrop_copy_policy",
            json::Value{std::string{material_backdrop_copy_policy()}});
        out.emplace(
            "backdrop_copy_required",
            json::Value{material_executor_requires_frame_capture(summary)});
        out.emplace(
            "backdrop_copy_skipped_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.backdrop_copy_skipped_count)});
        out.emplace(
            "execution_stage_count",
            json::Value{
                static_cast<std::int64_t>(summary.execution_stage_count)});
        out.emplace(
            "active_execution_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.active_execution_stage_count)});
        out.emplace(
            "backdrop_execution_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.backdrop_execution_stage_count)});
        out.emplace(
            "primary_execution_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.primary_execution_stage_count)});
        out.emplace(
            "dropped_execution_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.dropped_execution_stage_count)});
        out.emplace(
            "paint_layer_count",
            json::Value{
                static_cast<std::int64_t>(summary.paint_layer_count)});
        out.emplace(
            "active_paint_layer_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.active_paint_layer_count)});
        out.emplace(
            "dropped_paint_layer_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.dropped_paint_layer_count)});
        out.emplace(
            "shadow_paint_layer_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.shadow_paint_layer_count)});
        out.emplace(
            "fill_paint_layer_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.fill_paint_layer_count)});
        out.emplace(
            "edge_paint_layer_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.edge_paint_layer_count)});
        out.emplace(
            "max_paint_layer_inflate",
            json::Value{summary.max_paint_layer_inflate});
        out.emplace(
            "backdrop_filter_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.backdrop_filter_stage_count)});
        out.emplace(
            "fallback_fill_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.fallback_fill_stage_count)});
        out.emplace(
            "shadow_stage_count",
            json::Value{
                static_cast<std::int64_t>(summary.shadow_stage_count)});
        out.emplace(
            "edge_highlight_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.edge_highlight_stage_count)});
        out.emplace(
            "noise_dither_stage_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.noise_dither_stage_count)});
        out.emplace(
            "backdrop_access_plan_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.backdrop_access_plan_count)});
        out.emplace(
            "next_frame_capture_plan_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.next_frame_capture_plan_count)});
        out.emplace(
            "planned_frame_capture_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.planned_frame_capture_count)});
        out.emplace(
            "planned_frame_capture_pixels",
            json::Value{summary.planned_frame_capture_pixels});
        out.emplace(
            "planned_surface_sample_pixels",
            json::Value{summary.planned_surface_sample_pixels});
        out.emplace(
            "material_max_sample_taps",
            json::Value{
                static_cast<std::int64_t>(
                    summary.material_max_sample_taps)});
        out.emplace(
            "material_total_sample_taps",
            json::Value{summary.material_total_sample_taps});
        out.emplace(
            "backdrop_copy_pixels",
            json::Value{summary.backdrop_copy_pixels});
        out.emplace(
            "backdrop_copy_excludes_foreground_text",
            json::Value{summary.backdrop_copy_excludes_foreground_text});
        out.emplace(
            "foreground_pass_after_backdrop_copy",
            json::Value{summary.foreground_pass_after_backdrop_copy});
        out.emplace(
            "material_upload_bytes",
            json::Value{summary.material_upload_bytes});
        out.emplace(
            "material_buffer_capacity_bytes",
            json::Value{summary.material_buffer_capacity_bytes});
        out.emplace(
            "material_buffer_reallocations",
            json::Value{
                static_cast<std::int64_t>(
                    summary.material_buffer_reallocations)});
        out.emplace(
            "execution_contract_satisfied_count",
            json::Value{static_cast<std::int64_t>(
                summary.execution_contract_satisfied_count)});
        out.emplace(
            "execution_contract_mismatch_count",
            json::Value{static_cast<std::int64_t>(
                summary.execution_contract_mismatch_count)});
        out.emplace(
            "execution_contract_mismatch_total",
            json::Value{static_cast<std::int64_t>(
                summary.execution_contract_mismatch_total)});
        out.emplace(
            "stage_order_match_count",
            json::Value{static_cast<std::int64_t>(
                summary.stage_order_match_count)});
        out.emplace(
            "stage_order_mismatch_count",
            json::Value{static_cast<std::int64_t>(
                summary.stage_order_mismatch_count)});
        out.emplace(
            "first_execution_contract_mismatch",
            json::Value{summary.first_execution_contract_mismatch
                            ? summary.first_execution_contract_mismatch
                            : "none"});
        out.emplace(
            "first_stage_order_mismatch",
            json::Value{summary.first_stage_order_mismatch
                            ? summary.first_stage_order_mismatch
                            : "none"});
        out.emplace(
            "foreground_text_candidate_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.foreground_text_candidate_count)});
        out.emplace(
            "foreground_text_remap_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.foreground_text_remap_count)});
        out.emplace(
            "interaction_enabled_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_enabled_count)});
        out.emplace(
            "interaction_active_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_active_count)});
        out.emplace(
            "interaction_specular_highlight_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_specular_highlight_count)});
        out.emplace(
            "interaction_pointer_lens_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.interaction_pointer_lens_count)});
        out.emplace(
            "refraction_active_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.refraction_active_count)});
        out.emplace(
            "max_interaction_response_strength",
            json::Value{summary.max_interaction_response_strength});
        out.emplace(
            "max_interaction_specular_radius",
            json::Value{summary.max_interaction_specular_radius});
        out.emplace(
            "max_interaction_specular_intensity",
            json::Value{summary.max_interaction_specular_intensity});
        out.emplace(
            "max_interaction_pointer_lens_radius",
            json::Value{summary.max_interaction_pointer_lens_radius});
        out.emplace(
            "max_interaction_pointer_lens_strength",
            json::Value{summary.max_interaction_pointer_lens_strength});
        out.emplace(
            "max_refraction_strength",
            json::Value{summary.max_refraction_strength});
        out.emplace(
            "max_refraction_edge_bias",
            json::Value{summary.max_refraction_edge_bias});
        out.emplace(
            "max_refraction_offset_pixels",
            json::Value{summary.max_refraction_offset_pixels});
        out.emplace(
            "backdrop_descriptor_color_available",
            json::Value{summary.backdrop_descriptor_color_available});
        out.emplace(
            "backdrop_descriptor_color_mean",
            color_to_json(summary.backdrop_descriptor_color_mean));
        out.emplace(
            "backdrop_descriptor_color_sample_count",
            json::Value{static_cast<std::int64_t>(
                summary.backdrop_descriptor_color_sample_count)});
        out.emplace(
            "backdrop_descriptor_color_status",
            json::Value{summary.backdrop_descriptor_color_status
                            ? summary.backdrop_descriptor_color_status
                            : "not-sampled"});
        out.emplace(
            "backdrop_descriptor_luma_available",
            json::Value{summary.backdrop_descriptor_luma_available});
        out.emplace(
            "backdrop_descriptor_luma_min",
            json::Value{summary.backdrop_descriptor_luma_min});
        out.emplace(
            "backdrop_descriptor_luma_max",
            json::Value{summary.backdrop_descriptor_luma_max});
        out.emplace(
            "backdrop_descriptor_luma_mean",
            json::Value{summary.backdrop_descriptor_luma_mean});
        out.emplace(
            "backdrop_descriptor_luma_sample_count",
            json::Value{static_cast<std::int64_t>(
                summary.backdrop_descriptor_luma_sample_count)});
        out.emplace(
            "backdrop_descriptor_luma_grid_width",
            json::Value{static_cast<std::int64_t>(
                summary.backdrop_descriptor_luma_grid_width)});
        out.emplace(
            "backdrop_descriptor_luma_grid_height",
            json::Value{static_cast<std::int64_t>(
                summary.backdrop_descriptor_luma_grid_height)});
        out.emplace(
            "backdrop_descriptor_luma_frame",
            json::Value{static_cast<std::int64_t>(
                summary.backdrop_descriptor_luma_frame)});
        out.emplace(
            "backdrop_descriptor_luma_status",
            json::Value{summary.backdrop_descriptor_luma_status
                            ? summary.backdrop_descriptor_luma_status
                            : "not-sampled"});
        out.emplace(
            "backdrop_descriptor_source",
            json::Value{summary.backdrop_descriptor_source
                            ? summary.backdrop_descriptor_source
                            : "none"});
        out.emplace(
            "material_pipeline_ready",
            json::Value{summary.material_pipeline_ready});
        out.emplace(
            "material_backdrop_source_ready",
            json::Value{summary.material_backdrop_source_ready});
        out.emplace(
            "material_sampled_backdrop_upload_required",
            json::Value{summary.material_sampled_backdrop_upload_required});
        out.emplace(
            "material_sampled_backdrop_draw_required",
            json::Value{summary.material_sampled_backdrop_draw_required});
        out.emplace(
            "material_sampled_backdrop_uploaded",
            json::Value{summary.material_sampled_backdrop_uploaded});
        out.emplace(
            "material_sampled_backdrop_drawn",
            json::Value{summary.material_sampled_backdrop_drawn});
        out.emplace(
            "material_upload_status",
            json::Value{summary.material_upload_status
                            ? summary.material_upload_status
                            : "not-needed"});
        out.emplace(
            "material_draw_status",
            json::Value{summary.material_draw_status
                            ? summary.material_draw_status
                            : "not-needed"});
        out.emplace(
            "backdrop_luma_sampling_skipped_count",
            json::Value{static_cast<std::int64_t>(
                summary.backdrop_luma_sampling_skipped_count)});
        out.emplace(
            "backdrop_luma_sampling_skip_reason",
            json::Value{summary.backdrop_luma_sampling_skip_reason
                            ? summary.backdrop_luma_sampling_skip_reason
                            : "none"});
        out.emplace(
            "backdrop_copy_skip_reason",
            json::Value{summary.backdrop_copy_skip_reason
                            ? summary.backdrop_copy_skip_reason
                            : "none"});
        out.emplace(
            "container_groups",
            material_container_group_summary_json(summary.container_groups));
        out.emplace(
            "material_shader_content_scale",
            json::Value{summary.material_shader_content_scale});
        out.emplace(
            "material_max_shader_blur_step_pixels",
            json::Value{summary.material_max_shader_blur_step_pixels});
        out.emplace("cpu_decode_ns", json::Value{summary.cpu_decode_ns});
        out.emplace(
            "cpu_material_upload_ns",
            json::Value{summary.cpu_material_upload_ns});
        out.emplace("cpu_total_ns", json::Value{summary.cpu_total_ns});
        return json::Value{std::move(out)};
    }

    inline json::Object empty_material_renderer_contract(
            std::string_view fallback_policy) {
        json::Object renderer;
        renderer.emplace("material_pipeline_ready", json::Value{false});
        renderer.emplace("material_backdrop_source_ready", json::Value{false});
        json::Object luma_descriptor;
        luma_descriptor.emplace("available", json::Value{false});
        luma_descriptor.emplace("luma_min", json::Value{0.0});
        luma_descriptor.emplace("luma_max", json::Value{1.0});
        luma_descriptor.emplace("luma_mean", json::Value{0.5});
        luma_descriptor.emplace("sample_count", json::Value{std::int64_t{0}});
        luma_descriptor.emplace(
            "sample_grid_width",
            json::Value{std::int64_t{0}});
        luma_descriptor.emplace(
            "sample_grid_height",
            json::Value{std::int64_t{0}});
        luma_descriptor.emplace("sample_frame", json::Value{std::int64_t{0}});
        luma_descriptor.emplace("status", json::Value{"unsupported-fallback"});
        luma_descriptor.emplace("pending", json::Value{false});
        luma_descriptor.emplace(
            "skipped_sample_count",
            json::Value{std::int64_t{0}});
        renderer.emplace(
            "material_backdrop_luma_descriptor",
            json::Value{std::move(luma_descriptor)});
        renderer.emplace(
            "material_plan_contract_version",
            json::Value{
                static_cast<std::int64_t>(
                    material_plan_contract_version)});
        renderer.emplace("material_plan_count", json::Value{std::int64_t{0}});
        renderer.emplace("material_plans", json::Value{json::Array{}});
        renderer.emplace(
            "material_container_groups",
            json::Value{json::Array{}});
        renderer.emplace(
            "material_runtime_summary",
            material_runtime_summary_json(MaterialRuntimeSummary{}));
        renderer.emplace(
            "material_executor_summary",
            material_executor_summary_json(MaterialExecutorSummary{}));
        renderer.emplace(
            "material_fallback_policy",
            json::Value{std::string(fallback_policy)});
        return renderer;
    }

    inline void append_u16_le(std::vector<std::uint8_t>& out, std::uint16_t value) {
        out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
    }

    inline void append_u32_le(std::vector<std::uint8_t>& out, std::uint32_t value) {
        out.push_back(static_cast<std::uint8_t>(value & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFFu));
        out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFFu));
    }

    inline bool ensure_artifact_directory(std::filesystem::path const& directory,
                                          std::string& error) {
        std::error_code ec;
        if (directory.empty()) {
            error = "Artifact directory is empty";
            return false;
        }
        if (std::filesystem::create_directories(directory, ec)
            || std::filesystem::exists(directory)) {
            return true;
        }
        error = ec ? ec.message() : "Failed to create artifact directory";
        return false;
    }

    inline bool write_artifact_text_file(std::filesystem::path const& path,
                                         std::string_view contents,
                                         std::string& error) {
        auto parent = path.parent_path();
        if (!parent.empty() && !ensure_artifact_directory(parent, error))
            return false;

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "Failed to open file for writing: " + path.string();
            return false;
        }
        out.write(contents.data(), static_cast<std::streamsize>(contents.size()));
        if (!out.good()) {
            error = "Failed to write file: " + path.string();
            return false;
        }
        return true;
    }

    inline bool write_artifact_bmp_rgba(std::filesystem::path const& path,
                                        ArtifactFrameCapture const& frame,
                                        std::string& error) {
        if (frame.width == 0 || frame.height == 0) {
            error = "Frame capture is empty";
            return false;
        }
        auto expected_size =
            static_cast<std::size_t>(frame.width)
            * static_cast<std::size_t>(frame.height) * 4u;
        if (frame.rgba.size() != expected_size) {
            error = "Frame capture size does not match width/height";
            return false;
        }

        std::vector<std::uint8_t> bytes;
        bytes.reserve(14 + 40 + frame.rgba.size());

        auto file_size = static_cast<std::uint32_t>(14 + 40 + frame.rgba.size());
        auto pixel_offset = static_cast<std::uint32_t>(14 + 40);

        bytes.push_back(static_cast<std::uint8_t>('B'));
        bytes.push_back(static_cast<std::uint8_t>('M'));
        append_u32_le(bytes, file_size);
        append_u16_le(bytes, 0);
        append_u16_le(bytes, 0);
        append_u32_le(bytes, pixel_offset);

        append_u32_le(bytes, 40);
        append_u32_le(bytes, frame.width);
        append_u32_le(bytes, frame.height);
        append_u16_le(bytes, 1);
        append_u16_le(bytes, 32);
        append_u32_le(bytes, 0);
        append_u32_le(bytes, static_cast<std::uint32_t>(frame.rgba.size()));
        append_u32_le(bytes, 2835);
        append_u32_le(bytes, 2835);
        append_u32_le(bytes, 0);
        append_u32_le(bytes, 0);

        auto row_stride = static_cast<std::size_t>(frame.width) * 4u;
        for (unsigned int y = 0; y < frame.height; ++y) {
            auto src_y = frame.height - 1u - y;
            auto const* src_row = frame.rgba.data()
                + static_cast<std::size_t>(src_y) * row_stride;
            for (unsigned int x = 0; x < frame.width; ++x) {
                auto pixel_offset_bytes = static_cast<std::size_t>(x) * 4u;
                bytes.push_back(src_row[pixel_offset_bytes + 2]);
                bytes.push_back(src_row[pixel_offset_bytes + 1]);
                bytes.push_back(src_row[pixel_offset_bytes + 0]);
                bytes.push_back(src_row[pixel_offset_bytes + 3]);
            }
        }

        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (!out) {
            error = "Failed to open frame image for writing: " + path.string();
            return false;
        }
        out.write(
            reinterpret_cast<char const*>(bytes.data()),
            static_cast<std::streamsize>(bytes.size()));
        if (!out.good()) {
            error = "Failed to write frame image: " + path.string();
            return false;
        }
        return true;
    }

    inline ArtifactBundleResult write_debug_artifact_bundle(
            std::string_view directory,
            std::string_view platform,
            std::string_view snapshot_json,
            std::string_view platform_runtime_json = {},
            ArtifactFrameCapture const* frame = nullptr) {
        ArtifactBundleResult result{};
        result.directory = std::string(directory);

        auto directory_path = std::filesystem::path{result.directory};
        if (!ensure_artifact_directory(directory_path, result.error))
            return result;

        auto snapshot_path = directory_path / "snapshot.json";
        if (!write_artifact_text_file(snapshot_path, snapshot_json, result.error))
            return result;

        result.snapshot_json_path = snapshot_path.string();

        if (!platform.empty() && !platform_runtime_json.empty()) {
            auto platform_directory = directory_path / "platform";
            if (!ensure_artifact_directory(platform_directory, result.error))
                return result;

            auto runtime_path =
                platform_directory / (std::string(platform) + "-runtime.json");
            if (!write_artifact_text_file(
                    runtime_path,
                    platform_runtime_json,
                    result.error)) {
                return result;
            }
            result.platform_files.push_back(runtime_path.string());
        }

        if (frame != nullptr) {
            auto frame_path = directory_path / "frame.bmp";
            if (!write_artifact_bmp_rgba(frame_path, *frame, result.error))
                return result;
            result.frame_image_path = frame_path.string();
        }

        result.ok = true;
        return result;
    }
} // namespace detail

inline json::Value rect_to_json(RectSnapshot const& rect) {
    json::Object out;
    out.emplace("valid", json::Value{rect.valid});
    out.emplace("x", json::Value{rect.x});
    out.emplace("y", json::Value{rect.y});
    out.emplace("w", json::Value{rect.w});
    out.emplace("h", json::Value{rect.h});
    return json::Value{std::move(out)};
}

inline json::Value callback_id_to_json(std::optional<unsigned int> callback_id) {
    if (!callback_id.has_value())
        return json::Value{};
    return json::Value{static_cast<std::int64_t>(*callback_id)};
}

inline json::Value input_debug_to_json(InputDebugSnapshot const& snapshot) {
    json::Object out;
    out.emplace("event", json::Value{snapshot.event});
    out.emplace("source", json::Value{snapshot.source});
    out.emplace("detail", json::Value{snapshot.detail});
    out.emplace("result", json::Value{snapshot.result});
    out.emplace("callback_id", json::Value{static_cast<std::int64_t>(snapshot.callback_id)});
    out.emplace("role", json::Value{snapshot.role});
    out.emplace("focused_id", json::Value{static_cast<std::int64_t>(snapshot.focused_id)});
    out.emplace("focused_role", json::Value{snapshot.focused_role});
    out.emplace("focus_visible", json::Value{snapshot.focus_visible});
    out.emplace("input_modality", json::Value{snapshot.input_modality});
    out.emplace(
        "focus_visibility_reason",
        json::Value{snapshot.focus_visibility_reason});
    out.emplace("hovered_id", json::Value{static_cast<std::int64_t>(snapshot.hovered_id)});
    out.emplace("pressed_id", json::Value{static_cast<std::int64_t>(snapshot.pressed_id)});
    out.emplace("scroll_x", json::Value{snapshot.scroll_x});
    out.emplace("scroll_y", json::Value{snapshot.scroll_y});
    out.emplace("caret_pos", json::Value{static_cast<std::int64_t>(snapshot.caret_pos)});
    out.emplace("selection_active", json::Value{snapshot.selection_active});
    out.emplace(
        "selection_start",
        json::Value{static_cast<std::int64_t>(snapshot.selection_start)});
    out.emplace(
        "selection_end",
        json::Value{static_cast<std::int64_t>(snapshot.selection_end)});
    out.emplace("caret_visible", json::Value{snapshot.caret_visible});
    out.emplace("caret_renderer", json::Value{snapshot.caret_renderer});
    out.emplace("caret_rect", rect_to_json(snapshot.caret_rect));
    out.emplace("caret_draw_rect", rect_to_json(snapshot.caret_draw_rect));
    out.emplace("caret_host_rect", rect_to_json(snapshot.caret_host_rect));
    out.emplace("caret_screen_rect", rect_to_json(snapshot.caret_screen_rect));
    out.emplace("caret_host_bounds", rect_to_json(snapshot.caret_host_bounds));
    out.emplace("caret_host_flipped", json::Value{snapshot.caret_host_flipped});
    out.emplace("caret_geometry_source", json::Value{snapshot.caret_geometry_source});
    out.emplace("focused_is_input", json::Value{snapshot.focused_is_input});
    out.emplace("composition_active", json::Value{snapshot.composition_active});
    out.emplace("composition_text", json::Value{snapshot.composition_text});
    out.emplace(
        "composition_cursor",
        json::Value{static_cast<std::int64_t>(snapshot.composition_cursor)});
    return json::Value{std::move(out)};
}

inline json::Value semantic_node_to_json(SemanticNodeSnapshot const& node) {
    json::Object out;
    out.emplace("callback_id", callback_id_to_json(node.callback_id));
    out.emplace("role", json::Value{node.role});
    out.emplace("label", json::Value{node.label});
    if (node.material.has_value()) {
        json::Object material;
        material.emplace("kind", json::Value{node.material->kind});
        material.emplace("role", json::Value{node.material->role});
        material.emplace("opacity", json::Value{node.material->opacity});
        material.emplace("blur_radius", json::Value{node.material->blur_radius});
        json::Object tint;
        tint.emplace(
            "r",
            json::Value{static_cast<std::int64_t>(node.material->tint.r)});
        tint.emplace(
            "g",
            json::Value{static_cast<std::int64_t>(node.material->tint.g)});
        tint.emplace(
            "b",
            json::Value{static_cast<std::int64_t>(node.material->tint.b)});
        tint.emplace(
            "a",
            json::Value{static_cast<std::int64_t>(node.material->tint.a)});
        material.emplace("tint", json::Value{std::move(tint)});
        material.emplace("saturation", json::Value{node.material->saturation});
        material.emplace(
            "luminance_floor",
            json::Value{node.material->luminance_floor});
        material.emplace(
            "luminance_gain",
            json::Value{node.material->luminance_gain});
        material.emplace(
            "edge_highlight",
            json::Value{node.material->edge_highlight});
        material.emplace("edge_width", json::Value{node.material->edge_width});
        material.emplace(
            "noise_opacity",
            json::Value{node.material->noise_opacity});
        material.emplace("shadow_alpha", json::Value{node.material->shadow_alpha});
        material.emplace("shadow_radius", json::Value{node.material->shadow_radius});
        material.emplace(
            "container",
            detail::material_container_descriptor_json(
                node.material->container));
        json::Object prominence;
        prominence.emplace(
            "enabled",
            json::Value{node.material->prominence.enabled});
        prominence.emplace(
            "intensity",
            json::Value{node.material->prominence.intensity});
        material.emplace("prominence", json::Value{std::move(prominence)});
        material.emplace("fallback", json::Value{node.material->fallback});
        material.emplace(
            "fallback_reason",
            json::Value{node.material->fallback_reason});
        material.emplace(
            "contrast_intent",
            json::Value{node.material->contrast_intent});
        material.emplace("plan_id", json::Value{node.material->plan_id});
        material.emplace(
            "verifier_profile",
            json::Value{node.material->verifier_profile});
        out.emplace("material", json::Value{std::move(material)});
    } else {
        out.emplace("material", json::Value{});
    }
    out.emplace("bounds", rect_to_json(node.bounds));
    out.emplace("visible", json::Value{node.visible});
    out.emplace("enabled", json::Value{node.enabled});
    out.emplace("focusable", json::Value{node.focusable});
    out.emplace("focused", json::Value{node.focused});
    out.emplace("scroll_container", json::Value{node.scroll_container});
    json::Array children;
    for (auto const& child : node.children)
        children.push_back(semantic_node_to_json(child));
    out.emplace("children", json::Value{std::move(children)});
    return json::Value{std::move(out)};
}

inline json::Value system_settings_to_json(
        PlatformSystemSettingsSnapshot const& settings) {
    auto const availability = theme_contract_system_snapshot(settings);
    json::Object system;
    system.emplace("source", json::Value{settings.source});
    system.emplace("font_family", json::Value{settings.font_family});
    system.emplace("font_family_source", json::Value{settings.font_family_source});
    system.emplace("body_font_size", json::Value{settings.body_font_size});
    system.emplace("heading_font_size", json::Value{settings.heading_font_size});
    system.emplace("small_font_size", json::Value{settings.small_font_size});
    system.emplace("line_height_ratio", json::Value{settings.line_height_ratio});
    system.emplace("font_scale", json::Value{settings.font_scale});
    system.emplace("text_size_source", json::Value{settings.text_size_source});
    system.emplace(
        "font_weight_adjustment",
        json::Value{static_cast<std::int64_t>(
            settings.font_weight_adjustment)});
    system.emplace("font_weight_source", json::Value{settings.font_weight_source});
    system.emplace(
        "preferred_scroller_style",
        json::Value{settings.preferred_scroller_style});
    system.emplace("overlay_scrollbars", json::Value{settings.overlay_scrollbars});
    system.emplace("scroll_line_height", json::Value{settings.scroll_line_height});
    system.emplace("scroll_wheel_lines", json::Value{settings.scroll_wheel_lines});
    system.emplace("scroll_page_mode", json::Value{settings.scroll_page_mode});
    system.emplace(
        "scroll_vertical_factor",
        json::Value{settings.scroll_vertical_factor});
    system.emplace(
        "scroll_horizontal_factor",
        json::Value{settings.scroll_horizontal_factor});
    system.emplace("scroll_bar_size", json::Value{settings.scroll_bar_size});
    system.emplace("touch_slop", json::Value{settings.touch_slop});
    system.emplace("scroll_friction", json::Value{settings.scroll_friction});
    system.emplace(
        "scroll_delta_multiplier",
        json::Value{settings.scroll_delta_multiplier});
    system.emplace(
        "scroll_horizontal_delta_multiplier",
        json::Value{settings.scroll_horizontal_delta_multiplier});
    system.emplace("scroll_source", json::Value{settings.scroll_source});
    system.emplace(
        "font_family_available",
        json::Value{availability.font_family_available});
    system.emplace(
        "font_metrics_available",
        json::Value{availability.font_metrics_available});
    system.emplace(
        "font_scale_available",
        json::Value{availability.font_scale_available});
    system.emplace(
        "line_height_available",
        json::Value{availability.line_height_available});
    system.emplace(
        "scroll_metrics_available",
        json::Value{availability.scroll_metrics_available});
    system.emplace(
        "color_scheme_available",
        json::Value{availability.color_scheme_available});
    system.emplace(
        "double_click_interval_ms",
        json::Value{settings.double_click_interval_ms});
    system.emplace(
        "key_repeat_delay_ms",
        json::Value{settings.key_repeat_delay_ms});
    system.emplace(
        "key_repeat_interval_ms",
        json::Value{settings.key_repeat_interval_ms});
    system.emplace(
        "caret_blink_interval_ms",
        json::Value{settings.caret_blink_interval_ms});
    system.emplace(
        "input_timing_source",
        json::Value{settings.input_timing_source});
    system.emplace("preferred_locale", json::Value{settings.preferred_locale});
    system.emplace(
        "preferred_locale_source",
        json::Value{settings.preferred_locale_source});
    system.emplace("color_scheme", json::Value{settings.color_scheme});
    system.emplace(
        "color_scheme_source",
        json::Value{settings.color_scheme_source});
    system.emplace("appearance_name", json::Value{settings.appearance_name});
    system.emplace(
        "reduce_transparency",
        json::Value{settings.reduce_transparency});
    system.emplace(
        "increase_contrast",
        json::Value{settings.increase_contrast});
    system.emplace("reduce_motion", json::Value{settings.reduce_motion});
    system.emplace(
        "accessibility_source",
        json::Value{settings.accessibility_source});
    system.emplace(
        "accent_color_available",
        json::Value{settings.accent_color_available});
    system.emplace(
        "accent_color",
        detail::color_to_json(settings.accent_color));
    system.emplace(
        "accent_color_source",
        json::Value{settings.accent_color_source});
    system.emplace(
        "accent_color_opaque",
        json::Value{settings.accent_color_opaque});
    return json::Value{std::move(system)};
}

inline json::Value platform_capabilities_to_json(
        PlatformCapabilitiesSnapshot const& capabilities) {
    json::Object out;
    out.emplace("platform", json::Value{capabilities.platform});
    out.emplace("read_only", json::Value{capabilities.read_only});
    out.emplace("snapshot_json", json::Value{capabilities.snapshot_json});
    out.emplace("capture_frame_rgba", json::Value{capabilities.capture_frame_rgba});
    out.emplace(
        "write_artifact_bundle",
        json::Value{capabilities.write_artifact_bundle});
    out.emplace("semantic_tree", json::Value{capabilities.semantic_tree});
    out.emplace("input_debug", json::Value{capabilities.input_debug});
    out.emplace("platform_runtime", json::Value{capabilities.platform_runtime});
    out.emplace("frame_image", json::Value{capabilities.frame_image});
    out.emplace(
        "platform_diagnostics",
        json::Value{capabilities.platform_diagnostics});
    out.emplace(
        "material_surfaces",
        json::Value{capabilities.material_surfaces});
    out.emplace(
        "material_backdrop_blur",
        json::Value{capabilities.material_backdrop_blur});
    out.emplace(
        "material_shader_blur",
        json::Value{capabilities.material_shader_blur});
    out.emplace(
        "material_frame_history",
        json::Value{capabilities.material_frame_history});
    out.emplace(
        "material_max_shader_sample_taps",
        json::Value{static_cast<std::int64_t>(
            capabilities.material_max_shader_sample_taps)});
    out.emplace(
        "material_max_texture_dimension_2d",
        json::Value{capabilities.material_max_texture_dimension_2d});
    out.emplace(
        "material_max_backdrop_pixels",
        json::Value{capabilities.material_max_backdrop_pixels});
    out.emplace(
        "material_capability_profile",
        json::Value{capabilities.material_capability_profile});
    out.emplace(
        "material_capability_source",
        json::Value{capabilities.material_capability_source});
    out.emplace(
        "reduce_transparency",
        json::Value{capabilities.reduce_transparency});
    out.emplace(
        "increase_contrast",
        json::Value{capabilities.increase_contrast});
    out.emplace("reduce_motion", json::Value{capabilities.reduce_motion});
    out.emplace(
        "system_settings",
        system_settings_to_json(capabilities.system_settings));
    return json::Value{std::move(out)};
}

inline json::Value platform_runtime_to_json(
        PlatformRuntimeSnapshot const& runtime) {
    json::Object out;
    out.emplace("platform", json::Value{runtime.platform});
    out.emplace("backend", json::Value{runtime.backend});
    out.emplace("viewport", rect_to_json(runtime.viewport));
    out.emplace("scroll_x", json::Value{runtime.scroll_x});
    out.emplace("scroll_y", json::Value{runtime.scroll_y});
    out.emplace("content_height", json::Value{runtime.content_height});
    out.emplace(
        "focused_callback_id",
        callback_id_to_json(runtime.focused_callback_id));
    out.emplace("focus_visible", json::Value{runtime.focus_visible});
    out.emplace("input_modality", json::Value{runtime.input_modality});
    out.emplace(
        "focus_visibility_reason",
        json::Value{runtime.focus_visibility_reason});
    out.emplace(
        "hovered_callback_id",
        callback_id_to_json(runtime.hovered_callback_id));
    out.emplace(
        "pressed_callback_id",
        callback_id_to_json(runtime.pressed_callback_id));
    out.emplace("details", runtime.details);
    return json::Value{std::move(out)};
}

inline json::Value debug_plane_snapshot_to_json(
        DebugPlaneSnapshot const& snapshot) {
    json::Object out;
    out.emplace(
        "platform_capabilities",
        platform_capabilities_to_json(snapshot.platform_capabilities));
    out.emplace("input_debug", input_debug_to_json(snapshot.input_debug));
    if (snapshot.semantic_tree.has_value()) {
        out.emplace("semantic_tree", semantic_node_to_json(*snapshot.semantic_tree));
    } else {
        out.emplace("semantic_tree", json::Value{});
    }
    out.emplace(
        "platform_runtime",
        platform_runtime_to_json(snapshot.platform_runtime));
    return json::Value{std::move(out)};
}

inline void set_application_debug_provider(
        json::Value (*provider)()) noexcept {
    detail::set_application_debug_provider(provider);
}

inline json::Value attributes_to_json(std::vector<metrics::Attribute> const& attrs) {
    json::Object out;
    for (auto const& a : attrs) out.emplace(a.key, json::Value{a.value});
    return json::Value{std::move(out)};
}

inline json::Value counter_to_json(metrics::Counter const& c, std::uint64_t now) {
    json::Object obj;
    obj.emplace("name",         json::Value{c.name()});
    obj.emplace("description",  json::Value{c.description()});
    obj.emplace("unit",         json::Value{c.unit()});
    obj.emplace("type",         json::Value{"sum"});
    obj.emplace("is_monotonic", json::Value{true});
    json::Array points;
    auto start = metrics::detail::start_time_ns();
    for (auto const& dp : c.data_points()) {
        json::Object p;
        p.emplace("attributes",           attributes_to_json(dp.attributes));
        p.emplace("start_time_unix_nano", json::Value{static_cast<std::int64_t>(start)});
        p.emplace("time_unix_nano",       json::Value{static_cast<std::int64_t>(now)});
        p.emplace("value",                json::Value{static_cast<std::int64_t>(dp.value)});
        points.push_back(json::Value{std::move(p)});
    }
    obj.emplace("data_points", json::Value{std::move(points)});
    return json::Value{std::move(obj)};
}

inline json::Value gauge_to_json(metrics::Gauge const& g, std::uint64_t now) {
    json::Object obj;
    obj.emplace("name",        json::Value{g.name()});
    obj.emplace("description", json::Value{g.description()});
    obj.emplace("unit",        json::Value{g.unit()});
    obj.emplace("type",        json::Value{"gauge"});
    json::Array points;
    for (auto const& dp : g.data_points()) {
        json::Object p;
        p.emplace("attributes",     attributes_to_json(dp.attributes));
        p.emplace("time_unix_nano", json::Value{static_cast<std::int64_t>(now)});
        p.emplace("value",          json::Value{dp.value});
        points.push_back(json::Value{std::move(p)});
    }
    obj.emplace("data_points", json::Value{std::move(points)});
    return json::Value{std::move(obj)};
}

inline json::Value histogram_to_json(metrics::Histogram const& h, std::uint64_t now) {
    json::Object obj;
    obj.emplace("name",        json::Value{h.name()});
    obj.emplace("description", json::Value{h.description()});
    obj.emplace("unit",        json::Value{h.unit()});
    obj.emplace("type",        json::Value{"histogram"});
    json::Array bounds;
    for (double b : metrics::Histogram::bounds_ns) bounds.push_back(json::Value{b});
    obj.emplace("explicit_bounds", json::Value{std::move(bounds)});
    auto start = metrics::detail::start_time_ns();
    json::Array points;
    for (auto const& dp : h.data_points()) {
        json::Object p;
        p.emplace("attributes",           attributes_to_json(dp.attributes));
        p.emplace("start_time_unix_nano", json::Value{static_cast<std::int64_t>(start)});
        p.emplace("time_unix_nano",       json::Value{static_cast<std::int64_t>(now)});
        p.emplace("count",                json::Value{static_cast<std::int64_t>(dp.count)});
        p.emplace("sum",                  json::Value{static_cast<std::int64_t>(dp.sum)});
        json::Array buckets;
        for (auto b : dp.buckets) buckets.push_back(json::Value{static_cast<std::int64_t>(b)});
        p.emplace("bucket_counts", json::Value{std::move(buckets)});
        points.push_back(json::Value{std::move(p)});
    }
    obj.emplace("data_points", json::Value{std::move(points)});
    return json::Value{std::move(obj)};
}

inline json::Value resource_to_json() {
    json::Object o;
    o.emplace("service.name",    json::Value{"phenotype"});
#ifdef EXON_PKG_VERSION
    o.emplace("service.version", json::Value{std::string{EXON_PKG_VERSION}});
#else
    o.emplace("service.version", json::Value{"unknown"});
#endif
#if defined(__wasi__)
    o.emplace("runtime.name",    json::Value{"wasm32-wasi"});
#else
    o.emplace("runtime.name",    json::Value{"native"});
#endif
    return json::Value{std::move(o)};
}

inline json::Value logs_to_json() {
    json::Array arr;
    auto& r = ::phenotype::log::detail::ring();
    // Walk the ring oldest -> newest
    for (std::size_t i = 0; i < r.count; ++i) {
        std::size_t idx = (r.head + ::phenotype::log::detail::LOG_RING_CAPACITY - r.count + i)
            % ::phenotype::log::detail::LOG_RING_CAPACITY;
        auto const& rec = r.entries[idx];
        json::Object o;
        o.emplace("time_unix_nano",  json::Value{static_cast<std::int64_t>(rec.time_unix_nano)});
        o.emplace("severity_number", json::Value{static_cast<std::int64_t>(rec.severity)});
        o.emplace("severity_text",   json::Value{::phenotype::log::severity_text(rec.severity)});
        o.emplace("scope_name",      json::Value{rec.scope_name});
        o.emplace("body",            json::Value{rec.body});
        arr.push_back(json::Value{std::move(o)});
    }
    return json::Value{std::move(arr)};
}

inline json::Value build_snapshot() {
    auto now = metrics::detail::now_ns();
    using namespace ::phenotype::metrics;

    json::Array counters;
    counters.push_back(counter_to_json(inst::rebuilds, now));
    counters.push_back(counter_to_json(inst::alloc_nodes, now));
    counters.push_back(counter_to_json(inst::arena_resets, now));
    counters.push_back(counter_to_json(inst::messages_posted, now));
    counters.push_back(counter_to_json(inst::messages_drained, now));
    counters.push_back(counter_to_json(inst::input_events, now));
    counters.push_back(counter_to_json(inst::flush_calls, now));
    counters.push_back(counter_to_json(inst::frames_skipped, now));
    counters.push_back(counter_to_json(inst::measure_text_calls, now));
    counters.push_back(counter_to_json(inst::measure_text_cache_hits, now));
    counters.push_back(counter_to_json(inst::layout_nodes_skipped, now));
    counters.push_back(counter_to_json(inst::layout_nodes_computed, now));
    counters.push_back(counter_to_json(inst::paint_subtrees_blitted, now));
    counters.push_back(counter_to_json(inst::paint_bytes_blitted, now));
    counters.push_back(counter_to_json(inst::scissor_emitted, now));
    counters.push_back(counter_to_json(inst::keyed_lists_matched, now));
    counters.push_back(counter_to_json(inst::keyed_children_matched, now));
    counters.push_back(counter_to_json(inst::native_text_cache_hits, now));
    counters.push_back(counter_to_json(inst::native_text_cache_misses, now));
    counters.push_back(counter_to_json(inst::native_texture_upload_bytes, now));
    counters.push_back(counter_to_json(inst::native_buffer_reallocations, now));

    json::Array gauges;
    gauges.push_back(gauge_to_json(inst::live_nodes, now));
    gauges.push_back(gauge_to_json(inst::max_nodes_seen, now));
    gauges.push_back(gauge_to_json(inst::max_queue_depth, now));

    json::Array histograms;
    histograms.push_back(histogram_to_json(inst::frame_duration, now));
    histograms.push_back(histogram_to_json(inst::phase_duration, now));
    histograms.push_back(histogram_to_json(inst::native_phase_duration, now));

    json::Object root;
    root.emplace("resource",   resource_to_json());
    root.emplace("logs",       logs_to_json());
    root.emplace("counters",   json::Value{std::move(counters)});
    root.emplace("gauges",     json::Value{std::move(gauges)});
    root.emplace("histograms", json::Value{std::move(histograms)});
    if (auto builder = detail::current_debug_payload_builder()) {
        root.emplace("debug", builder());
    } else {
        root.emplace("debug", debug_plane_snapshot_to_json({}));
    }
    return json::Value{std::move(root)};
}

inline std::string serialize_snapshot() {
    return json::emit(build_snapshot());
}

} // namespace phenotype::diag

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
        out.emplace("interactive", json::Value{analysis.interactive});
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
            "max_container_spacing",
            json::Value{plan.resource_budget.max_container_spacing});
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
            execution_stages.push_back(json::Value{std::move(out_stage)});
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
        out.emplace("reference_model", json::Value{std::move(reference_model)});
        out.emplace("plan_id", json::Value{plan.plan_id});
        out.emplace(
            "command_descriptor",
            json::Value{std::move(command_descriptor)});
        out.emplace("geometry", json::Value{std::move(geometry)});
        out.emplace("shape", json::Value{std::move(shape)});
        out.emplace("render_target", json::Value{std::move(render_target)});
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
        out.emplace("verifier", json::Value{std::move(verifier)});
        out.emplace(
            "observation_contract",
            json::Value{std::move(observation_contract)});
        out.emplace("passes", json::Value{std::move(passes)});
        out.emplace("execution_stages", json::Value{std::move(execution_stages)});
        return json::Value{std::move(out)};
    }

    inline json::Array material_plans_runtime_json(
            std::vector<MaterialRuntimeRecord> const& records) {
        json::Array plans;
        for (auto const& record : records)
            plans.push_back(material_plan_runtime_json(record));
        return plans;
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
            "theme_default_glass_token_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.theme_default_glass_token_count)});
        out.emplace(
            "theme_custom_token_count",
            json::Value{
                static_cast<std::int64_t>(
                    summary.theme_custom_token_count)});
        out.emplace("max_surface_area",
                    json::Value{summary.max_surface_area});
        out.emplace("max_effective_radius",
                    json::Value{summary.max_effective_radius});
        out.emplace("max_radius_limit",
                    json::Value{summary.max_radius_limit});
        out.emplace("max_normalized_radius",
                    json::Value{summary.max_normalized_radius});
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
        return json::Value{std::move(out)};
    }

    inline json::Value material_runtime_summary_json(
            std::vector<MaterialRuntimeRecord> const& records) {
        MaterialRuntimeSummary summary;
        for (auto const& record : records)
            accumulate_material_runtime_summary(summary, record);
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

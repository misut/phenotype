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
    unsigned int hovered_id = 0xFFFFFFFFu;
    float scroll_y = 0.0f;
    unsigned int caret_pos = 0xFFFFFFFFu;
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
};

struct PlatformRuntimeSnapshot {
    std::string platform;
    std::string backend;
    RectSnapshot viewport{};
    float scroll_y = 0.0f;
    float content_height = 0.0f;
    std::optional<unsigned int> focused_callback_id;
    std::optional<unsigned int> hovered_callback_id;
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
    using PlatformCapabilitiesProvider = PlatformCapabilitiesSnapshot (*)();
    using PlatformRuntimeDetailsProvider = json::Value (*)();

    DebugPayloadBuilder& debug_payload_builder_storage() noexcept {
        static DebugPayloadBuilder builder = nullptr;
        return builder;
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

    void set_platform_capabilities_provider(PlatformCapabilitiesProvider provider) noexcept {
        platform_capabilities_provider_storage() = provider;
    }

    void set_platform_runtime_details_provider(PlatformRuntimeDetailsProvider provider) noexcept {
        platform_runtime_details_provider_storage() = provider;
    }

    DebugPayloadBuilder current_debug_payload_builder() noexcept {
        return debug_payload_builder_storage();
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
    out.emplace("hovered_id", json::Value{static_cast<std::int64_t>(snapshot.hovered_id)});
    out.emplace("scroll_y", json::Value{snapshot.scroll_y});
    out.emplace("caret_pos", json::Value{static_cast<std::int64_t>(snapshot.caret_pos)});
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
    return json::Value{std::move(out)};
}

inline json::Value platform_runtime_to_json(
        PlatformRuntimeSnapshot const& runtime) {
    json::Object out;
    out.emplace("platform", json::Value{runtime.platform});
    out.emplace("backend", json::Value{runtime.backend});
    out.emplace("viewport", rect_to_json(runtime.viewport));
    out.emplace("scroll_y", json::Value{runtime.scroll_y});
    out.emplace("content_height", json::Value{runtime.content_height});
    out.emplace(
        "focused_callback_id",
        callback_id_to_json(runtime.focused_callback_id));
    out.emplace(
        "hovered_callback_id",
        callback_id_to_json(runtime.hovered_callback_id));
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

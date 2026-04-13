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
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <format>
#include <limits>
#include <map>
#include <memory>
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
inline void trace(char const* scope, std::format_string<Args...> fmt, Args&&... args) {
    if (Severity::trace < current_level()) return;
    detail::emit_formatted(Severity::trace, scope,
        std::format(fmt, std::forward<Args>(args)...));
}
template<typename... Args>
inline void debug(char const* scope, std::format_string<Args...> fmt, Args&&... args) {
    if (Severity::debug < current_level()) return;
    detail::emit_formatted(Severity::debug, scope,
        std::format(fmt, std::forward<Args>(args)...));
}
template<typename... Args>
inline void info(char const* scope, std::format_string<Args...> fmt, Args&&... args) {
    if (Severity::info < current_level()) return;
    detail::emit_formatted(Severity::info, scope,
        std::format(fmt, std::forward<Args>(args)...));
}
template<typename... Args>
inline void warn(char const* scope, std::format_string<Args...> fmt, Args&&... args) {
    if (Severity::warn < current_level()) return;
    detail::emit_formatted(Severity::warn, scope,
        std::format(fmt, std::forward<Args>(args)...));
}
template<typename... Args>
inline void error(char const* scope, std::format_string<Args...> fmt, Args&&... args) {
    if (Severity::error < current_level()) return;
    detail::emit_formatted(Severity::error, scope,
        std::format(fmt, std::forward<Args>(args)...));
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
        timespec ts;
        std::timespec_get(&ts, TIME_UTC);
        return static_cast<std::uint64_t>(ts.tv_sec) * 1'000'000'000ULL
             + static_cast<std::uint64_t>(ts.tv_nsec);
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
    "Input events received by WASM exports (attribute: event)",
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

inline Histogram& frame_duration = *new Histogram{
    "phenotype.runner.frame_duration",
    "Total frame duration (drain + update + view + layout + paint + flush)",
    "ns"};
inline Histogram& phase_duration = *new Histogram{
    "phenotype.runner.phase_duration",
    "Per-phase duration inside one rebuild (attribute: phase)",
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
    inst::frame_duration.reset();
    inst::phase_duration.reset();
    // NOTE: g_app.last_paint_hash is NOT reset here — phenotype.diag
    // cannot import phenotype.state (cycle: state already imports diag).
    // Tests that need a clean hash state assign detail::g_app.last_paint_hash
    // = 0 directly themselves.
    auto& ring = ::phenotype::log::detail::ring();
    ring.head = 0;
    ring.count = 0;
}

} // namespace phenotype::metrics

namespace phenotype::diag {

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

    json::Array gauges;
    gauges.push_back(gauge_to_json(inst::live_nodes, now));
    gauges.push_back(gauge_to_json(inst::max_nodes_seen, now));
    gauges.push_back(gauge_to_json(inst::max_queue_depth, now));

    json::Array histograms;
    histograms.push_back(histogram_to_json(inst::frame_duration, now));
    histograms.push_back(histogram_to_json(inst::phase_duration, now));

    json::Object root;
    root.emplace("resource",   resource_to_json());
    root.emplace("logs",       logs_to_json());
    root.emplace("counters",   json::Value{std::move(counters)});
    root.emplace("gauges",     json::Value{std::move(gauges)});
    root.emplace("histograms", json::Value{std::move(histograms)});
    return json::Value{std::move(root)};
}

inline std::string serialize_snapshot() {
    return json::emit(build_snapshot());
}

} // namespace phenotype::diag

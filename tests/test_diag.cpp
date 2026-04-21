// test_diag — phenotype.diag instrument + JSON snapshot tests.
//
// Counters / gauges / histograms are validated directly through the
// instrument API, then the snapshot is round-tripped through jsoncpp
// to verify the wire format that the JS-side adapter will consume.

#include <cassert>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <variant>
import phenotype;
import json;
#ifdef __wasi__
import phenotype.wasm;
#endif

using namespace phenotype;

#if !defined(__wasi__) && !defined(__ANDROID__)
static null_host diag_host;
#else
extern "C" {
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int, char const*, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#endif

namespace {

// Helper: locate an instrument by name in a JSON array of metric objects.
json::Object const* find_metric(json::Array const& arr, std::string_view name) {
    for (auto const& v : arr) {
        if (!v.is_object()) continue;
        auto const& obj = v.as_object();
        auto it = obj.find("name");
        if (it == obj.end() || !it->second.is_string()) continue;
        if (it->second.as_string() == name) return &obj;
    }
    return nullptr;
}

json::Object const* find_semantic_child(json::Array const& arr,
                                        std::string_view role,
                                        std::string_view label = {}) {
    for (auto const& value : arr) {
        if (!value.is_object())
            continue;
        auto const& obj = value.as_object();
        auto role_it = obj.find("role");
        if (role_it == obj.end() || !role_it->second.is_string())
            continue;
        if (role_it->second.as_string() != role)
            continue;
        if (label.empty())
            return &obj;
        auto label_it = obj.find("label");
        if (label_it != obj.end() && label_it->second.is_string()
            && label_it->second.as_string() == label) {
            return &obj;
        }
    }
    return nullptr;
}

int count_semantic_role(json::Array const& arr, std::string_view role) {
    int count = 0;
    for (auto const& value : arr) {
        if (!value.is_object())
            continue;
        auto const& obj = value.as_object();
        auto it = obj.find("role");
        if (it != obj.end() && it->second.is_string() && it->second.as_string() == role)
            ++count;
    }
    return count;
}

std::filesystem::path make_debug_bundle_dir(std::string_view label) {
    auto stamp = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto path = std::filesystem::path("phenotype-diag-" + std::string(label) + "-" + stamp);
    std::filesystem::remove_all(path);
    return path;
}

std::string read_text_file(std::filesystem::path const& path) {
    std::ifstream in(path, std::ios::binary);
    assert(in.is_open());
    return std::string(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

} // namespace

void test_counter_add_and_total() {
    metrics::reset_all();
    auto& c = metrics::inst::rebuilds;
    assert(c.total() == 0);
    c.add();
    c.add(5);
    assert(c.total() == 6);
}

void test_counter_attribute_split() {
    metrics::reset_all();
    auto& c = metrics::inst::input_events;
    c.add(1, {{"event", "click"}});
    c.add(2, {{"event", "click"}});
    c.add(1, {{"event", "key"}});
    // Two distinct attribute combinations.
    assert(c.data_points().size() == 2);
    assert(c.total() == 4);
}

void test_gauge_set_and_max() {
    metrics::reset_all();
    auto& g = metrics::inst::live_nodes;
    g.set(10);
    assert(g.data_points().size() == 1);
    assert(g.data_points()[0].value == 10);
    g.set(7);
    assert(g.data_points()[0].value == 7);

    auto& m = metrics::inst::max_nodes_seen;
    m.update_max(3);
    m.update_max(8);
    m.update_max(5);
    assert(m.data_points()[0].value == 8);
}

void test_histogram_record_buckets() {
    metrics::reset_all();
    auto& h = metrics::inst::frame_duration;
    h.record(1'000'000);     //  1 ms  -> bucket 0 (<= 5ms)
    h.record(20'000'000);    // 20 ms  -> bucket 2 (<= 25ms)
    h.record(20'000'000'000); // 20 s  -> overflow bucket
    assert(h.data_points().size() == 1);
    auto const& dp = h.data_points()[0];
    assert(dp.count == 3);
    assert(dp.sum == 1'000'000ULL + 20'000'000ULL + 20'000'000'000ULL);
    assert(dp.buckets[0] == 1);
    assert(dp.buckets[2] == 1);
    // Overflow bucket sits at bounds_ns.size()
    assert(dp.buckets[metrics::Histogram::bounds_ns.size()] == 1);
}

void test_log_level_filter() {
    metrics::reset_all();
    log::set_level(log::Severity::warn);
    log::info("test", "info should be filtered");

    auto& ring = log::detail::ring();
    assert(ring.count == 0);

    log::warn("test", "warn should land");
    assert(ring.count == 1);
    assert(ring.entries[0].severity == log::Severity::warn);
    assert(ring.entries[0].body == std::string{"warn should land"});

    log::set_level(log::Severity::info);
}

void test_log_ring_wraps() {
    metrics::reset_all();
    log::set_level(log::Severity::trace);
    auto& ring = log::detail::ring();
    constexpr std::size_t cap = log::detail::LOG_RING_CAPACITY;
    for (std::size_t i = 0; i < cap + 5; ++i)
        log::info("test", "msg {}", i);
    // Ring caps at capacity; excess overwrites the oldest entries.
    assert(ring.count == cap);
    log::set_level(log::Severity::info);
}

void test_snapshot_shape() {
    metrics::reset_all();
    log::set_level(log::Severity::info);
    metrics::inst::rebuilds.add();
    metrics::inst::input_events.add(1, {{"event", "click"}});
    metrics::inst::frame_duration.record(8'000'000); // 8ms
    metrics::inst::native_text_cache_hits.add(2, {{"platform", "macos"}});
    metrics::inst::native_phase_duration.record(
        1'000'000,
        {{"platform", "macos"}, {"phase", "command_decode"}});
    log::info("test", "hello {}", 42);

    auto json_str = detail::serialize_diag_snapshot_with_debug();
    assert(!json_str.empty());

    auto v = json::parse(json_str);
    assert(v.is_object());
    auto const& root = v.as_object();
    assert(root.contains("resource"));
    assert(root.contains("logs"));
    assert(root.contains("counters"));
    assert(root.contains("gauges"));
    assert(root.contains("histograms"));
    assert(root.contains("debug"));

    auto const& debug = root.at("debug").as_object();
    assert(debug.contains("platform_capabilities"));
    assert(debug.contains("input_debug"));
    assert(debug.contains("semantic_tree"));
    assert(debug.contains("platform_runtime"));

    auto const& capabilities = debug.at("platform_capabilities").as_object();
    assert(capabilities.at("read_only").as_bool() == true);
    assert(capabilities.at("snapshot_json").as_bool() == true);
    assert(capabilities.contains("capture_frame_rgba"));
    assert(capabilities.contains("write_artifact_bundle"));
    assert(capabilities.at("input_debug").as_bool() == true);
    assert(capabilities.at("semantic_tree").as_bool() == true);
    assert(capabilities.at("platform_runtime").as_bool() == true);
    assert(capabilities.contains("frame_image"));
    assert(capabilities.contains("platform_diagnostics"));

    auto const& input_debug = debug.at("input_debug").as_object();
    assert(input_debug.contains("event"));
    assert(input_debug.contains("caret_rect"));

    auto const& runtime = debug.at("platform_runtime").as_object();
#ifdef __wasi__
    assert(capabilities.at("platform").as_string() == "wasi");
    assert(capabilities.at("capture_frame_rgba").as_bool() == false);
    assert(capabilities.at("write_artifact_bundle").as_bool() == true);
    assert(capabilities.at("frame_image").as_bool() == false);
    assert(capabilities.at("platform_diagnostics").as_bool() == false);
    assert(runtime.at("backend").as_string() == "wasi");
    auto const& runtime_details = runtime.at("details").as_object();
    assert(runtime_details.at("host_model").as_string() == "wasi");
    assert(runtime_details.at("frame_capture_supported").as_bool() == false);
#else
    assert(runtime.at("backend").as_string() == "native");
#endif
    assert(runtime.contains("viewport"));

    // Resource carries service.name + service.version.
    auto const& resource = root.at("resource").as_object();
    assert(resource.at("service.name").as_string() == "phenotype");

    // rebuilds counter is monotonic and shows up with our value.
    auto const& counters = root.at("counters").as_array();
    auto const* rebuilds = find_metric(counters, "phenotype.runner.rebuilds");
    assert(rebuilds != nullptr);
    assert(rebuilds->at("type").as_string() == "sum");
    assert(rebuilds->at("is_monotonic").as_bool() == true);
    auto const& rb_points = rebuilds->at("data_points").as_array();
    assert(rb_points.size() == 1);
    assert(rb_points[0].as_object().at("value").as_integer() == 1);

    // input_events carries the click attribute.
    auto const* events = find_metric(counters, "phenotype.input.events");
    assert(events != nullptr);
    auto const& ev_points = events->at("data_points").as_array();
    assert(ev_points.size() == 1);
    auto const& ev_attrs = ev_points[0].as_object().at("attributes").as_object();
    assert(ev_attrs.at("event").as_string() == "click");

    auto const* native_hits = find_metric(counters, "phenotype.native.text_cache_hits");
    assert(native_hits != nullptr);
    auto const& native_hit_points = native_hits->at("data_points").as_array();
    assert(native_hit_points.size() == 1);
    auto const& native_hit_attrs = native_hit_points[0].as_object().at("attributes").as_object();
    assert(native_hit_attrs.at("platform").as_string() == "macos");

    // Frame histogram has 14 explicit bounds and one populated data point.
    auto const& histograms = root.at("histograms").as_array();
    auto const* frame = find_metric(histograms, "phenotype.runner.frame_duration");
    assert(frame != nullptr);
    auto const& bounds = frame->at("explicit_bounds").as_array();
    assert(bounds.size() == metrics::Histogram::bounds_ns.size());
    auto const& fd_points = frame->at("data_points").as_array();
    assert(fd_points.size() == 1);
    auto const& fd0 = fd_points[0].as_object();
    assert(fd0.at("count").as_integer() == 1);
    auto const& fd_buckets = fd0.at("bucket_counts").as_array();
    // bounds_ns.size() + 1 buckets including the +Inf overflow.
    assert(fd_buckets.size() == metrics::Histogram::bounds_ns.size() + 1);

    auto const* native_phase = find_metric(histograms, "phenotype.native.phase_duration");
    assert(native_phase != nullptr);
    auto const& native_phase_points = native_phase->at("data_points").as_array();
    assert(native_phase_points.size() == 1);
    auto const& native_phase_attrs =
        native_phase_points[0].as_object().at("attributes").as_object();
    assert(native_phase_attrs.at("platform").as_string() == "macos");
    assert(native_phase_attrs.at("phase").as_string() == "command_decode");

    // Logs round-trip with severity_text and body.
    auto const& logs = root.at("logs").as_array();
    bool found_log = false;
    for (auto const& l : logs) {
        auto const& lo = l.as_object();
        if (lo.at("body").as_string() == "hello 42") {
            assert(lo.at("severity_text").as_string() == "INFO");
            assert(lo.at("severity_number").as_integer()
                   == static_cast<std::int64_t>(log::Severity::info));
            found_log = true;
        }
    }
    assert(found_log);
}

// Smoke-test the runner: a single run<> pass should populate the
// rebuild counter and the per-phase histogram with one record per phase.
struct DiagState {};
struct DiagMsg {};
struct DebugPlaneNoop {};
struct DebugPlaneTextChanged { std::string value; };
using DebugPlaneMsg = std::variant<DebugPlaneNoop, DebugPlaneTextChanged>;

static DebugPlaneMsg map_debug_plane_text(std::string value) {
    return DebugPlaneTextChanged{std::move(value)};
}

void test_runner_records_phases() {
    metrics::reset_all();
    log::set_level(log::Severity::info);
#if !defined(__wasi__) && !defined(__ANDROID__)
    run<DiagState, DiagMsg>(diag_host,
#else
    run<DiagState, DiagMsg>(
#endif
        [](DiagState const&) {
            widget::text({"diag", 4});
        },
        [](DiagState&, DiagMsg) {});

    assert(metrics::inst::rebuilds.total() >= 1);
    assert(metrics::inst::frame_duration.data_points().size() == 1);
    auto const& fd = metrics::inst::frame_duration.data_points()[0];
    assert(fd.count == metrics::inst::rebuilds.total());

    // phase_duration carries one data point per phase attribute value.
    // Five phases recorded by the runner: update / view / layout / paint / flush.
    auto const& phase_points = metrics::inst::phase_duration.data_points();
    assert(phase_points.size() == 5);
}

void test_debug_plane_semantic_tree_shape_and_stability() {
    metrics::reset_all();
    log::set_level(log::Severity::info);
#if !defined(__wasi__) && !defined(__ANDROID__)
    run<DiagState, DebugPlaneMsg>(diag_host,
#else
    run<DiagState, DebugPlaneMsg>(
#endif
        [](DiagState const&) {
            layout::column([&] {
                widget::text("Heading");
                widget::button<DebugPlaneMsg>("Run", DebugPlaneNoop{});
                widget::link("Docs", "https://example.com/docs");
                widget::checkbox<DebugPlaneMsg>("Subscribe", true, DebugPlaneNoop{});
                widget::radio<DebugPlaneMsg>("Option A", true, DebugPlaneNoop{});
                widget::text_field<DebugPlaneMsg>(
                    "Type here",
                    "",
                    map_debug_plane_text);
                widget::image("hero.png", 48.0f, 32.0f);
            });
        },
        [](DiagState&, DebugPlaneMsg) {});

    auto first = json::parse(detail::serialize_diag_snapshot_with_debug());
    auto const& debug = first.as_object().at("debug").as_object();
    auto const& semantic_tree = debug.at("semantic_tree").as_object();
    assert(semantic_tree.at("role").as_string() == "root");
    assert(semantic_tree.at("scroll_container").as_bool() == true);
    assert(semantic_tree.at("visible").as_bool() == true);
    auto const& root_bounds = semantic_tree.at("bounds").as_object();
    assert(root_bounds.at("valid").as_bool() == true);

    auto const& children = semantic_tree.at("children").as_array();
    assert(children.size() == 7);
    assert(count_semantic_role(children, "checkbox") == 1);
    assert(count_semantic_role(children, "radio") == 1);
    assert(count_semantic_role(children, "button") == 1);
    assert(count_semantic_role(children, "link") == 1);
    assert(count_semantic_role(children, "text") == 1);
    assert(count_semantic_role(children, "text_field") == 1);
    assert(count_semantic_role(children, "image") == 1);

    auto const* checkbox = find_semantic_child(children, "checkbox", "Subscribe");
    assert(checkbox != nullptr);
    assert(checkbox->at("callback_id").as_integer() >= 0);
    assert(checkbox->at("focusable").as_bool() == true);
    assert(checkbox->at("enabled").as_bool() == true);
    assert(checkbox->at("visible").as_bool() == true);

    auto const* radio = find_semantic_child(children, "radio", "Option A");
    assert(radio != nullptr);
    assert(radio->at("callback_id").as_integer() >= 0);
    assert(radio->at("focusable").as_bool() == true);

    auto const* button = find_semantic_child(children, "button", "Run");
    assert(button != nullptr);
    assert(button->at("callback_id").as_integer() >= 0);

    auto const* link = find_semantic_child(children, "link", "Docs");
    assert(link != nullptr);
    assert(link->at("callback_id").as_integer() >= 0);

    auto const* text = find_semantic_child(children, "text", "Heading");
    assert(text != nullptr);
    assert(text->at("focusable").as_bool() == false);

    auto const* text_field = find_semantic_child(children, "text_field", "Type here");
    assert(text_field != nullptr);
    assert(text_field->at("callback_id").as_integer() >= 0);
    assert(text_field->at("focusable").as_bool() == true);

    auto const* image = find_semantic_child(children, "image");
    assert(image != nullptr);
    assert(image->at("focusable").as_bool() == false);

    auto const& runtime = debug.at("platform_runtime").as_object();
    auto const& viewport = runtime.at("viewport").as_object();
    assert(viewport.at("valid").as_bool() == true);
    assert(viewport.contains("w"));
    assert(viewport.contains("h"));

    auto first_tree = json::emit(debug.at("semantic_tree"));
    detail::trigger_rebuild();
    auto second = json::parse(detail::serialize_diag_snapshot_with_debug());
    auto second_tree =
        json::emit(second.as_object().at("debug").as_object().at("semantic_tree"));
    assert(first_tree == second_tree);
}

#ifdef __wasi__
void test_wasi_debug_artifact_bundle_contract() {
    auto bundle_dir = make_debug_bundle_dir("wasi");
    auto snapshot_json = detail::serialize_diag_snapshot_with_debug();
    auto bundle = phenotype::wasi::detail::write_artifact_bundle(
        bundle_dir.string(),
        snapshot_json,
        "wasi-common-contract-test");

    if (!bundle.ok && bundle.error == "No such file or directory") {
        std::puts("SKIP: WASI debug artifact bundle requires a writable preopened directory");
        return;
    }
    assert(bundle.ok);
    assert(bundle.frame_image_path.empty());
    assert(bundle.platform_files.size() == 1);
    assert(std::filesystem::exists(bundle_dir / "snapshot.json"));
    assert(std::filesystem::exists(bundle_dir / "platform" / "wasi-runtime.json"));

    auto bundle_snapshot = json::parse(read_text_file(bundle_dir / "snapshot.json"));
    auto const& debug = bundle_snapshot.as_object().at("debug").as_object();
    auto const& capabilities = debug.at("platform_capabilities").as_object();
    assert(capabilities.at("platform").as_string() == "wasi");
    assert(capabilities.at("write_artifact_bundle").as_bool() == true);
    assert(capabilities.at("capture_frame_rgba").as_bool() == false);

    auto runtime_file = json::parse(
        read_text_file(bundle_dir / "platform" / "wasi-runtime.json"));
    auto const& runtime_obj = runtime_file.as_object();
    assert(runtime_obj.at("host_model").as_string() == "wasi");
    assert(runtime_obj.at("frame_capture_supported").as_bool() == false);
    assert(runtime_obj.at("artifact_reason").as_string() == "wasi-common-contract-test");

    std::filesystem::remove_all(bundle_dir);
}
#endif

int main() {
    test_counter_add_and_total();
    std::printf("PASS: counter add + total\n");
    test_counter_attribute_split();
    std::printf("PASS: counter attribute split\n");
    test_gauge_set_and_max();
    std::printf("PASS: gauge set + update_max\n");
    test_histogram_record_buckets();
    std::printf("PASS: histogram record + bucket assignment\n");
    test_log_level_filter();
    std::printf("PASS: log level filter\n");
    test_log_ring_wraps();
    std::printf("PASS: log ring buffer wraps at capacity\n");
    test_snapshot_shape();
    std::printf("PASS: JSON snapshot shape\n");
    test_runner_records_phases();
    std::printf("PASS: runner records frame + phase histograms\n");
    test_debug_plane_semantic_tree_shape_and_stability();
    std::printf("PASS: debug plane semantic tree shape + stability\n");
#ifdef __wasi__
    test_wasi_debug_artifact_bundle_contract();
    std::printf("PASS: WASI debug artifact bundle contract\n");
#endif
    std::printf("\nAll diag tests passed.\n");
    return 0;
}

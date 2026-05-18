// test_diag — phenotype.diag instrument + JSON snapshot tests.
//
// Counters / gauges / histograms are validated directly through the
// instrument API, then the snapshot is round-tripped through jsoncpp
// to verify the wire format that the JS-side adapter will consume.

#include <cassert>
#include <chrono>
#include <cmath>
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
    float phenotype_measure_text(float fs, unsigned int /*flags*/,
                                  char const* /*family*/, unsigned int /*family_len*/,
                                  char const* /*text*/, unsigned int len) {
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

json::Object const* find_semantic_descendant(json::Object const& obj,
                                             std::string_view role,
                                             std::string_view label = {}) {
    auto role_it = obj.find("role");
    if (role_it != obj.end() && role_it->second.is_string()
        && role_it->second.as_string() == role) {
        if (label.empty())
            return &obj;
        auto label_it = obj.find("label");
        if (label_it != obj.end() && label_it->second.is_string()
            && label_it->second.as_string() == label) {
            return &obj;
        }
    }

    auto children_it = obj.find("children");
    if (children_it == obj.end() || !children_it->second.is_array())
        return nullptr;
    for (auto const& child : children_it->second.as_array()) {
        if (!child.is_object())
            continue;
        if (auto const* found =
                find_semantic_descendant(child.as_object(), role, label)) {
            return found;
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
    assert(capabilities.at("material_surfaces").as_bool() == true);
    assert(capabilities.at("material_backdrop_blur").as_bool() == false);
    assert(capabilities.at("material_shader_blur").as_bool() == false);
    assert(capabilities.at("material_frame_history").as_bool() == false);
    assert(capabilities.at("reduce_transparency").as_bool() == false);
    assert(capabilities.at("increase_contrast").as_bool() == false);
    assert(capabilities.at("reduce_motion").as_bool() == false);
    auto const& system_settings =
        capabilities.at("system_settings").as_object();
    assert(system_settings.at("source").is_string());
    assert(system_settings.at("font_family").is_string());
    (void)system_settings.at("body_font_size").as_float();
    (void)system_settings.at("font_scale").as_float();
    assert(system_settings.at("preferred_scroller_style").is_string());
    (void)system_settings.at("scroll_line_height").as_float();
    (void)system_settings.at("scroll_delta_multiplier").as_float();

    auto const& input_debug = debug.at("input_debug").as_object();
    assert(input_debug.contains("event"));
    assert(input_debug.contains("pressed_id"));
    assert(input_debug.contains("focus_visible"));
    assert(input_debug.contains("caret_rect"));

    auto const& runtime = debug.at("platform_runtime").as_object();
    assert(runtime.contains("pressed_callback_id"));
    assert(runtime.contains("focus_visible"));
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

#if !defined(__wasi__) && !defined(__ANDROID__)
// Single emit_* whose payload exceeds the backend's *initial* capacity
// (INIT_SIZE = 65 536) used to silently walk past the end of the
// fixed-size buffer (memory corruption). PR #220 turned that into a
// loud overflow + drop. The dynamic-grow follow-up (this slab) goes
// further: when the host can grow its backing store, the emit succeeds
// and no overflow metric ticks. This test covers both branches.
//
// `diag_abort_on_paint_overflow` is toggled off so the debug-build
// abort branch in `report_paint_overflow` is suppressed inside the
// cap-exceeded part; production code never touches that flag.
void test_paint_buffer_overflow_records_metric_and_drops_command() {
    detail::g_app.diag_abort_on_paint_overflow = false;

    // ---- Part 1: emit > INIT_SIZE but < MAX_SIZE.
    // The host's `reserve()` should grow `buffer_` past INIT_SIZE and
    // the emit should land — overflow metric must NOT tick.
    {
        metrics::reset_all();
        null_host host;
        PathBuilder p;
        p.move_to(0.0f, 0.0f);
        // FillPath header = 12 bytes; each line_to emits 3 verb words
        // = 12 bytes. 6000 line_to → ~72 KB > INIT_SIZE 65536, well
        // below MAX_SIZE 4 MiB. Single command so the pre-flush
        // shortcut can't help — only `reserve` can.
        for (int i = 1; i < 6000; ++i)
            p.line_to(static_cast<float>(i), static_cast<float>(i));

        Color const red{255, 0, 0, 255};
        emit_fill_path(host, p, red);

        // Buffer grew past INIT_SIZE, emit landed in full, no overflow.
        assert(host.buf_size() > null_host::INIT_SIZE);
        assert(host.buf_len() > null_host::INIT_SIZE);
        assert(metrics::inst::paint_buffer_overflow.total() == 0);
    }

    // ---- Part 2: emit > MAX_SIZE.
    // `reserve()` refuses past the cap; `ensure()` falls through to
    // the existing report-overflow + drop path with the opcode
    // attribute on the metric.
    {
        metrics::reset_all();
        null_host host;
        PathBuilder p;
        p.move_to(0.0f, 0.0f);
        // null_host::MAX_SIZE = 4 MiB. 12 bytes per line_to verb →
        // ≥ 349 526 line_to to exceed; round to 400 000 so the
        // FillPath header + closing alignment also clear the cap.
        for (int i = 1; i < 400000; ++i)
            p.line_to(static_cast<float>(i), static_cast<float>(i));

        Color const red{255, 0, 0, 255};
        emit_fill_path(host, p, red);

        // Buffer maxed out at MAX_SIZE, write dropped, metric ticked.
        assert(host.buf_size() <= null_host::MAX_SIZE);
        assert(metrics::inst::paint_buffer_overflow.total() >= 1);
        auto const& dps = metrics::inst::paint_buffer_overflow.data_points();
        bool found_fillpath = false;
        for (auto const& dp : dps) {
            for (auto const& attr : dp.attributes) {
                if (attr.key == "opcode" && attr.value == "FillPath")
                    found_fillpath = true;
            }
        }
        assert(found_fillpath);
    }

    detail::g_app.diag_abort_on_paint_overflow = true;
}
#endif

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
                widget::button<DebugPlaneMsg>(
                    "Disabled run",
                    DebugPlaneNoop{},
                    ButtonVariant::Default,
                    true);
                widget::link("Docs", "https://example.com/docs");
                widget::checkbox<DebugPlaneMsg>("Subscribe", true, DebugPlaneNoop{});
                widget::radio<DebugPlaneMsg>("Option A", true, DebugPlaneNoop{});
                widget::text_field<DebugPlaneMsg>(
                    "Type here",
                    "",
                    map_debug_plane_text);
                widget::text_field<DebugPlaneMsg>(
                    "Locked field",
                    "read only",
                    map_debug_plane_text,
                    false,
                    true);
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
    assert(children.size() == 9);
    assert(count_semantic_role(children, "checkbox") == 1);
    assert(count_semantic_role(children, "radio") == 1);
    assert(count_semantic_role(children, "button") == 2);
    assert(count_semantic_role(children, "link") == 1);
    assert(count_semantic_role(children, "text") == 1);
    assert(count_semantic_role(children, "text_field") == 2);
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

    auto const* disabled_button =
        find_semantic_child(children, "button", "Disabled run");
    assert(disabled_button != nullptr);
    assert(disabled_button->at("callback_id").is_null());
    assert(disabled_button->at("enabled").as_bool() == false);
    assert(disabled_button->at("focusable").as_bool() == false);

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

    auto const* disabled_text_field =
        find_semantic_child(children, "text_field", "Locked field");
    assert(disabled_text_field != nullptr);
    assert(disabled_text_field->at("callback_id").is_null());
    assert(disabled_text_field->at("enabled").as_bool() == false);
    assert(disabled_text_field->at("focusable").as_bool() == false);

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

void test_material_surface_semantic_debug_fields() {
    metrics::reset_all();
    log::set_level(log::Severity::info);
#if !defined(__wasi__) && !defined(__ANDROID__)
    run<DiagState, DebugPlaneMsg>(diag_host,
#else
    run<DiagState, DebugPlaneMsg>(
#endif
        [](DiagState const&) {
            layout::material_surface(MaterialKind::Regular, [&] {
                widget::text("Glass panel");
                widget::button<DebugPlaneMsg>("Action", DebugPlaneNoop{});
            });
        },
        [](DiagState&, DebugPlaneMsg) {});

    auto parsed = json::parse(detail::serialize_diag_snapshot_with_debug());
    auto const& debug = parsed.as_object().at("debug").as_object();
    auto const& capabilities = debug.at("platform_capabilities").as_object();
    assert(capabilities.at("material_surfaces").as_bool() == true);
    assert(capabilities.at("material_backdrop_blur").as_bool() == false);
    assert(capabilities.at("material_shader_blur").as_bool() == false);
    assert(capabilities.at("material_frame_history").as_bool() == false);
    assert(capabilities.at("reduce_transparency").as_bool() == false);
    assert(capabilities.at("increase_contrast").as_bool() == false);
    assert(capabilities.at("reduce_motion").as_bool() == false);

    auto const& semantic_tree = debug.at("semantic_tree").as_object();
    auto const& children = semantic_tree.at("children").as_array();
    auto const* material = find_semantic_child(children, "material");
    assert(material != nullptr);
    assert(material->contains("material"));
    auto const& material_debug = material->at("material").as_object();
    assert(material_debug.at("kind").as_string() == "regular");
    assert(material_debug.at("role").as_string() == "surface");
    assert(material_debug.at("container").as_object().at("mode").as_string()
           == "isolated");
    assert(material_debug.at("fallback").as_bool() == true);
    assert(material_debug.at("fallback_reason").as_string().find("fallback path")
           != std::string::npos);
    assert(material_debug.at("opacity").as_float() > 0.5);
    assert(material_debug.at("blur_radius").as_float() >= 20.0);
    auto const& tint = material_debug.at("tint").as_object();
    assert(tint.at("a").as_integer() > 0);
    assert(material_debug.at("saturation").as_float() > 1.0);
    assert(material_debug.at("edge_highlight").as_float() > 0.0);
    assert(material_debug.at("noise_opacity").as_float() > 0.0);
    assert(material_debug.at("shadow_alpha").as_float() > 0.0);
    assert(material_debug.at("contrast_intent").as_string() == "legible");
    assert(material_debug.at("plan_id").as_string() == "material.regular.base");
    assert(material_debug.at("verifier_profile").as_string()
           == "regular-legibility-backdrop");

    auto const& material_children = material->at("children").as_array();
    assert(find_semantic_child(material_children, "text", "Glass panel") != nullptr);
    assert(find_semantic_child(material_children, "button", "Action") != nullptr);
}

void test_material_app_chrome_helpers_are_semantic_materials() {
    metrics::reset_all();
    log::set_level(log::Severity::info);
#if !defined(__wasi__) && !defined(__ANDROID__)
    run<DiagState, DebugPlaneMsg>(diag_host,
#else
    run<DiagState, DebugPlaneMsg>(
#endif
        [](DiagState const&) {
            layout::column([&] {
                layout::toolbar([&] {
                    widget::button<DebugPlaneMsg>("New", DebugPlaneNoop{});
                    widget::button<DebugPlaneMsg>("Delete", DebugPlaneNoop{});
                });
                layout::toolbar(
                    layout::MaterialSurfaceOptions{
                        .kind = MaterialKind::Thick,
                        .direction = FlexDirection::Row,
                        .padding = SpaceToken::Xs,
                        .gap = SpaceToken::Xs,
                        .border_radius = 9.0f,
                        .border_width = 0.0f,
                        .semantic_label = "Compact Toolbar",
                    },
                    [&] {
                        widget::button<DebugPlaneMsg>("Compact",
                                                       DebugPlaneNoop{});
                    });
                layout::navigation([&] {
                    widget::button<DebugPlaneMsg>("Root", DebugPlaneNoop{});
                });
                layout::navigation(
                    layout::MaterialSurfaceOptions{
                        .kind = MaterialKind::Regular,
                        .direction = FlexDirection::Row,
                        .padding = SpaceToken::Xs,
                        .gap = SpaceToken::Xs,
                        .border_radius = 8.0f,
                        .border_width = 0.0f,
                        .semantic_label = "Compact Navigation",
                    },
                    [&] {
                        widget::button<DebugPlaneMsg>("Documents",
                                                       DebugPlaneNoop{});
                    });
                layout::sidebar(160.0f, [&] {
                    widget::text("Locations");
                });
                layout::status_bar([&] {
                    widget::text("Ready");
                }, MaterialKind::Thick);
                layout::status_bar(
                    layout::MaterialSurfaceOptions{
                        .kind = MaterialKind::Clear,
                        .padding = SpaceToken::Xs,
                        .gap = SpaceToken::Xs,
                        .border_radius = 4.0f,
                        .border_width = 0.0f,
                        .semantic_label = "Compact Status",
                    },
                    [&] {
                        widget::text("Compact Ready");
                    });
            });
        },
        [](DiagState&, DebugPlaneMsg) {});

    auto parsed = json::parse(detail::serialize_diag_snapshot_with_debug());
    auto const& children = parsed.as_object()
        .at("debug").as_object()
        .at("semantic_tree").as_object()
        .at("children").as_array();

    auto const* toolbar = find_semantic_child(children, "material", "Toolbar");
    auto const* compact_toolbar =
        find_semantic_child(children, "material", "Compact Toolbar");
    auto const* navigation = find_semantic_child(children, "material", "Navigation");
    auto const* compact_navigation =
        find_semantic_child(children, "material", "Compact Navigation");
    auto const* sidebar = find_semantic_child(children, "material", "Sidebar");
    auto const* status_bar = find_semantic_child(children, "material", "Status Bar");
    auto const* compact_status =
        find_semantic_child(children, "material", "Compact Status");
    assert(toolbar != nullptr);
    assert(compact_toolbar != nullptr);
    assert(navigation != nullptr);
    assert(compact_navigation != nullptr);
    assert(sidebar != nullptr);
    assert(status_bar != nullptr);
    assert(compact_status != nullptr);
    assert(toolbar->at("material").as_object().at("kind").as_string() == "clear");
    assert(toolbar->at("material").as_object().at("role").as_string() == "toolbar");
    assert(compact_toolbar->at("material").as_object().at("kind").as_string()
           == "thick");
    assert(compact_toolbar->at("material").as_object().at("role").as_string()
           == "toolbar");
    assert(navigation->at("material").as_object().at("kind").as_string()
           == "thin");
    assert(navigation->at("material").as_object().at("role").as_string()
           == "navigation");
    assert(compact_navigation->at("material").as_object().at("kind").as_string()
           == "regular");
    assert(compact_navigation->at("material").as_object().at("role").as_string()
           == "navigation");
    assert(sidebar->at("material").as_object().at("kind").as_string() == "thin");
    assert(sidebar->at("material").as_object().at("role").as_string() == "sidebar");
    assert(status_bar->at("material").as_object().at("kind").as_string()
           == "thick");
    assert(status_bar->at("material").as_object().at("role").as_string()
           == "status_bar");
    assert(compact_status->at("material").as_object().at("kind").as_string()
           == "clear");
    assert(compact_status->at("material").as_object().at("role").as_string()
           == "status_bar");
    assert(find_semantic_descendant(*toolbar, "button", "New") != nullptr);
    assert(find_semantic_descendant(*compact_toolbar, "button", "Compact")
           != nullptr);
    assert(find_semantic_descendant(*navigation, "button", "Root")
           != nullptr);
    assert(find_semantic_descendant(*compact_navigation, "button", "Documents")
           != nullptr);
    assert(find_semantic_descendant(*sidebar, "text", "Locations") != nullptr);
    assert(find_semantic_descendant(*status_bar, "text", "Ready") != nullptr);
    assert(find_semantic_descendant(*compact_status, "text", "Compact Ready")
           != nullptr);
}

void test_material_container_semantic_debug_fields() {
    metrics::reset_all();
    log::set_level(log::Severity::info);
#if !defined(__wasi__) && !defined(__ANDROID__)
    run<DiagState, DebugPlaneMsg>(diag_host,
#else
    run<DiagState, DebugPlaneMsg>(
#endif
        [](DiagState const&) {
            layout::material_container(
                layout::MaterialContainerOptions{
                    .container_id = 777u,
                    .union_id = 3u,
                    .spacing = 20.0f,
                    .interactive = true,
                    .morph_transitions = true,
                },
                [] {
                    layout::material_surface(MaterialKind::Thin, [] {
                        widget::text("Union glass");
                    });
                });
        },
        [](DiagState&, DebugPlaneMsg) {});

    auto parsed = json::parse(detail::serialize_diag_snapshot_with_debug());
    auto const& children = parsed.as_object()
        .at("debug").as_object()
        .at("semantic_tree").as_object()
        .at("children").as_array();

    auto const* material = find_semantic_child(children, "material");
    assert(material != nullptr);
    auto const& container = material->at("material").as_object()
        .at("container").as_object();
    assert(container.at("mode").as_string() == "union");
    assert(container.at("container_id").as_integer() == 777);
    assert(container.at("union_id").as_integer() == 3);
    assert(container.at("spacing").as_float() == 20.0f);
    assert(container.at("interactive").as_bool() == true);
    assert(container.at("morph_transitions").as_bool() == true);
    assert(find_semantic_descendant(*material, "text", "Union glass") != nullptr);
}

void test_overlay_semantic_debug_nodes_are_screen_fixed() {
    metrics::reset_all();
    log::set_level(log::Severity::info);
#if !defined(__wasi__) && !defined(__ANDROID__)
    run<DiagState, DebugPlaneMsg>(diag_host,
#else
    run<DiagState, DebugPlaneMsg>(
#endif
        [](DiagState const&) {
            layout::column([&] {
                widget::text("Main content");
                layout::spacer(900.0f);
            });
            layout::overlay([&] {
                layout::spacer(32.0f);
                layout::material_surface(MaterialKind::Thin, [&] {
                    widget::text("Overlay glass");
                    widget::button<DebugPlaneMsg>("Overlay action", DebugPlaneNoop{});
                });
            });
        },
        [](DiagState&, DebugPlaneMsg) {});

    detail::set_scroll_y(640.0f);
    auto parsed = json::parse(detail::serialize_diag_snapshot_with_debug());
    auto const& semantic_tree =
        parsed.as_object().at("debug").as_object().at("semantic_tree").as_object();
    auto const& children = semantic_tree.at("children").as_array();

    auto const* material = find_semantic_child(children, "material");
    assert(material != nullptr);
    assert(material->at("visible").as_bool() == true);
    auto const& material_debug = material->at("material").as_object();
    assert(material_debug.at("kind").as_string() == "thin");
    assert(material_debug.at("role").as_string() == "surface");
    assert(find_semantic_descendant(*material, "text", "Overlay glass") != nullptr);
    assert(find_semantic_descendant(*material, "button", "Overlay action") != nullptr);
}

void test_material_runtime_record_json_contract() {
    Theme theme{};
    MaterialEnvironment env{};
    env.capabilities.material_surfaces = true;
    env.capabilities.material_backdrop_blur = false;
    env.capabilities.shader_blur = false;
    env.capabilities.frame_history = false;
    env.backdrop.source = "test-fallback";
    env.render_target.width = 320;
    env.render_target.height = 240;
    env.render_target.scale = 2.0f;
    env.render_target.pixel_format = "rgba8unorm";
    env.debug_seed.frame = 7;
    env.debug_seed.node = 11;
    env.quality.max_blur_radius = 18.0f;
    env.quality.max_sample_taps = 9;
    env.quality.max_backdrop_pixels = 100'000;

    auto plan = plan_material_surface(
        material_request_for_command(
            MaterialKind::Thin,
            0.5f,
            14.0f,
            Color{240, 248, 255, 128},
            MaterialGeometry{4.0f, 8.0f, 160.0f, 64.0f, 12.0f},
            theme),
        env);
    MaterialRuntimeRecord record{plan, 3};
    auto value = diag::detail::material_plan_runtime_json(record);
    auto const& obj = value.as_object();

    assert(obj.at("command_index").as_integer() == 3);
    assert(obj.at("contract_version").as_integer()
           == material_plan_contract_version);
    assert(obj.at("kind").as_string() == "thin");
    assert(obj.at("role").as_string() == "surface");
    auto const& container = obj.at("container").as_object();
    assert(container.at("mode").as_string() == "isolated");
    assert(container.at("container_id").as_integer() == 0);
    assert(container.at("union_id").as_integer() == 0);
    assert(container.at("spacing").as_float() == 0.0f);
    assert(container.at("interactive").as_bool() == false);
    assert(container.at("morph_transitions").as_bool() == false);
    assert(container.at("participates").as_bool() == false);
    assert(container.at("shared_backdrop_scope").as_bool() == false);
    assert(container.at("shape_union_expected").as_bool() == false);
    auto const& reference_model = obj.at("reference_model").as_object();
    assert(reference_model.at("technology").as_string() == "liquid-glass");
    assert(reference_model.at("layer").as_string() == "functional-layer");
    assert(reference_model.at("material_policy").as_string()
           == "liquid-glass-functional-layer");
    assert(reference_model.at("variant").as_string() == "thin");
    assert(reference_model.at("shape").as_string() == "rounded-rectangle");
    assert(reference_model.at("shape_scope").as_string() == "view-bounds");
    assert(reference_model.at("blending_scope").as_string()
           == "deterministic-fallback");
    assert(reference_model.at("semantic_thickness").as_string() == "thin");
    assert(reference_model.at("accessibility_response").as_string()
           == "standard");
    assert(reference_model.at("performance_response").as_string()
           == "deterministic-fallback");
    assert(reference_model.at("view_bounds_anchored").as_bool() == true);
    assert(reference_model.at("shape_matches_geometry").as_bool() == true);
    assert(reference_model.at("tint_applied").as_bool() == true);
    assert(reference_model.at("interactive_response").as_bool() == false);
    assert(reference_model.at("container_grouped").as_bool() == false);
    assert(reference_model.at("container_union").as_bool() == false);
    assert(reference_model.at("container_morphing").as_bool() == false);
    assert(reference_model.at("legibility_preserved").as_bool() == true);
    assert(reference_model.at("vibrancy_expected").as_bool() == false);
    assert(reference_model.at("deterministic_degradation").as_bool() == true);
    auto const& shape = obj.at("shape").as_object();
    assert(shape.at("valid").as_bool() == true);
    assert(shape.at("kind").as_string() == "rounded-rectangle");
    assert(shape.at("rounded").as_bool() == true);
    assert(shape.at("capsule").as_bool() == false);
    assert(shape.at("radius_clamped").as_bool() == false);
    assert(shape.at("surface_area").as_float() == 160.0f * 64.0f);
    assert(shape.at("min_extent").as_float() == 64.0f);
    assert(shape.at("max_extent").as_float() == 160.0f);
    assert(shape.at("radius_limit").as_float() == 32.0f);
    assert(shape.at("effective_radius").as_float() == 12.0f);
    assert(std::fabs(shape.at("normalized_radius").as_float()
                     - (12.0f / 32.0f)) < 0.0001f);
    assert(obj.at("plan_id").as_string() == "material.thin.fallback");
    auto const& descriptor = obj.at("command_descriptor").as_object();
    assert(descriptor.at("kind").as_string() == "thin");
    assert(descriptor.at("role").as_string() == "surface");
    assert(descriptor.at("container").as_object().at("mode").as_string()
           == "isolated");
    assert(descriptor.at("opacity").as_float() == 0.5f);
    assert(descriptor.at("blur_radius").as_float() == 14.0f);
    auto const& descriptor_tint = descriptor.at("tint").as_object();
    assert(descriptor_tint.at("r").as_integer() == 240);
    assert(descriptor_tint.at("g").as_integer() == 248);
    assert(descriptor_tint.at("b").as_integer() == 255);
    assert(descriptor_tint.at("a").as_integer() == 128);
    assert(obj.at("fallback").as_bool() == true);
    assert(obj.at("fallback_path").as_string() == "unsupported-backend");
    assert(obj.at("fallback_reason").as_string()
           == "backend reports no material backdrop blur support");
    assert(obj.at("backdrop_sampling").as_bool() == false);
    assert(obj.at("backdrop").as_object()
               .at("excludes_foreground_text").as_bool() == false);
    assert(obj.at("backdrop_access").as_object()
               .at("excludes_foreground_text").as_bool() == false);
    assert(obj.at("sample_taps").as_integer() == 0);
    auto const& sampling_kernel = obj.at("sampling_kernel").as_object();
    assert(sampling_kernel.at("name").as_string() == "none");
    assert(sampling_kernel.at("radius").as_integer() == 0);
    assert(sampling_kernel.at("sample_taps").as_integer() == 0);
    assert(sampling_kernel.at("blur_step_scale").as_float() == 0.0f);
    assert(sampling_kernel.at("weight_profile").as_string() == "none");
    assert(sampling_kernel.at("requires_backdrop").as_bool() == false);
    assert(sampling_kernel.at("bounded").as_bool() == true);
    auto const& luminance_curve = obj.at("luminance_curve").as_object();
    assert(luminance_curve.at("name").as_string() == "fallback-flat");
    assert(luminance_curve.at("floor").as_float()
           == obj.at("luminance_floor").as_float());
    assert(luminance_curve.at("gain").as_float()
           == obj.at("luminance_gain").as_float());
    assert(luminance_curve.at("gamma").as_float() == 1.0f);
    assert(luminance_curve.at("midpoint").as_float() == 0.5f);
    assert(luminance_curve.at("contrast").as_float() == 1.0f);
    assert(luminance_curve.at("edge_lift").as_float()
           == obj.at("edge_highlight").as_float());
    assert(luminance_curve.at("backdrop_driven").as_bool() == false);
    assert(luminance_curve.at("bounded").as_bool() == true);
    auto const& foreground = obj.at("foreground").as_object();
    assert(foreground.at("scheme").as_string() == "solid-fallback");
    assert(foreground.at("source").as_string() == "fallback-material");
    assert(foreground.at("backdrop_driven").as_bool() == false);
    assert(foreground.at("high_contrast").as_bool() == false);
    assert(foreground.at("uses_vibrancy").as_bool() == false);
    assert(foreground.at("deterministic").as_bool() == true);
    assert(foreground.at("primary").as_object().at("a").as_integer() == 255);
    assert(foreground.at("primary_contrast_ratio").as_float()
           >= foreground.at("minimum_contrast_ratio").as_float());
    assert(foreground.at("secondary_contrast_ratio").as_float()
           >= foreground.at("minimum_contrast_ratio").as_float());
    assert(foreground.at("accent_contrast_ratio").as_float()
           >= foreground.at("minimum_contrast_ratio").as_float());
    auto const& decision_trace = obj.at("decision_trace").as_object();
    assert(decision_trace.at("has_material").as_bool() == true);
    assert(decision_trace.at("role_allows_liquid_glass").as_bool() == true);
    assert(decision_trace.at("content_layer_standard_material").as_bool()
           == false);
    assert(decision_trace.at("liquid_glass_backdrop_candidate").as_bool()
           == true);
    assert(decision_trace.at("target_ready").as_bool() == true);
    assert(decision_trace.at("backend_supports_backdrop").as_bool() == false);
    assert(decision_trace.at("can_sample_backdrop").as_bool() == false);
    assert(decision_trace.at("next_frame_capture_required").as_bool() == false);
    assert(decision_trace.at("reduced_transparency").as_bool() == false);
    assert(decision_trace.at("increase_contrast").as_bool() == false);
    assert(decision_trace.at("reduce_motion").as_bool() == false);
    assert(decision_trace.at("first_blocker").as_string()
           == "unsupported-backend");
    assert(obj.at("primary_pass").as_object().at("name").as_string()
           == "translucent-rounded-rect");
    assert(obj.at("primary_pass").as_object().at("sample_taps").as_integer()
           == 0);
    assert(obj.at("primary_pass").as_object().at("executor").as_string()
           == "fallback-fill");
    assert(obj.at("primary_pass").as_object()
               .at("max_texture_copy_pixels").as_integer() == 0);
    auto const& quality_policy = obj.at("quality_policy").as_object();
    assert(quality_policy.at("allow_backdrop_sampling").as_bool() == true);
    assert(quality_policy.at("allow_noise").as_bool() == true);
    assert(quality_policy.at("allow_shadow").as_bool() == true);
    assert(quality_policy.at("max_blur_radius").as_float() == 18.0);
    assert(quality_policy.at("max_sample_taps").as_integer() == 9);
    assert(quality_policy.at("max_backdrop_pixels").as_integer() == 100'000);
    assert(obj.at("resource_budget").as_object()
               .at("max_sample_taps").as_integer() == 9);
    assert(obj.at("resource_budget").as_object()
               .at("max_sampling_kernel_radius").as_integer() == 0);
    assert(obj.at("resource_budget").as_object()
               .at("max_container_spacing").as_float() == 0.0f);
    assert(obj.at("resource_budget").as_object()
               .at("deterministic_fallback").as_bool() == true);
    assert(obj.at("execution_stage_capacity").as_integer() == 4);
    assert(obj.at("dropped_execution_stage_count").as_integer() == 0);
    assert(obj.at("verifier").as_object().at("likely_layer").as_string()
           == "material-fallback-pass");
    auto const& observation =
        obj.at("observation_contract").as_object();
    assert(observation.at("schema_version").as_integer()
           == material_plan_contract_version);
    assert(observation.at("semantic_material_required").as_bool() == true);
    assert(observation.at("runtime_plan_required").as_bool() == true);
    assert(observation.at("fallback_expected").as_bool() == true);
    assert(observation.at("backdrop_sampling_expected").as_bool() == false);
    assert(observation.at("stable_backdrop_required").as_bool() == false);
    assert(observation.at("shared_frame_capture_required").as_bool() == false);
    assert(observation.at("next_frame_capture_required").as_bool() == false);
    assert(observation.at("backdrop_capture_excludes_foreground_text")
               .as_bool() == false);
    assert(observation.at("bounded_texture_copy_required").as_bool() == true);
    assert(observation.at("deterministic_fallback_required").as_bool()
           == true);
    assert(observation.at("fallback_path").as_string()
           == "unsupported-backend");
    assert(observation.at("fallback_reason").as_string()
           == "backend reports no material backdrop blur support");
    assert(observation.at("backdrop_capture_scope").as_string() == "none");
    assert(observation.at("backdrop_capture_reason").as_string()
           == "not-required");
    assert(observation.at("primary_pass").as_string()
           == "translucent-rounded-rect");
    assert(observation.at("primary_executor").as_string()
           == "fallback-fill");
    assert(observation.at("expected_runtime_passes").as_integer() == 1);
    assert(observation.at("expected_active_runtime_passes").as_integer() == 1);
    assert(observation.at("expected_backdrop_runtime_passes").as_integer()
           == 0);
    assert(observation.at("expected_execution_stages").as_integer() == 3);
    assert(observation.at("expected_active_execution_stages").as_integer()
           == 3);
    assert(observation.at("expected_backdrop_execution_stages").as_integer()
           == 0);
    assert(observation.at("max_texture_copy_pixels").as_integer() == 0);
    assert(observation.at("region_name").as_string()
           == "thin-balanced-backdrop");
    assert(observation.at("likely_layer").as_string()
           == "material-fallback-pass");
    assert(observation.at("likely_pass").as_string()
           == "translucent-rounded-rect");

    std::vector<MaterialRuntimeRecord> records{record};
    auto plans = diag::detail::material_plans_runtime_json(records);
    assert(plans.size() == 1);
    MaterialRuntimeSummary pure_summary;
    accumulate_material_runtime_summary(pure_summary, record);
    assert(pure_summary.plan_count == 1);
    assert(pure_summary.fallback_count == 1);
    assert(pure_summary.total_runtime_passes == 1);
    assert(pure_summary.active_runtime_passes == 1);
    assert(pure_summary.backdrop_runtime_passes == 0);
    assert(pure_summary.next_frame_capture_plan_count == 0);
    assert(pure_summary.dropped_execution_stages == 0);
    assert(pure_summary.max_execution_stage_capacity == 4);
    assert(pure_summary.max_pass_texture_copy_pixels == 0);
    assert(pure_summary.total_pass_texture_copy_pixels == 0);
    assert(pure_summary.max_plan_sample_taps == 0);
    assert(pure_summary.total_plan_sample_taps == 0);
    assert(pure_summary.max_sample_taps == 9);
    assert(pure_summary.max_sampling_kernel_radius == 0);
    assert(pure_summary.containered_count == 0);
    assert(pure_summary.unioned_count == 0);
    assert(pure_summary.valid_shape_count == 1);
    assert(pure_summary.rounded_shape_count == 1);
    assert(pure_summary.capsule_shape_count == 0);
    assert(pure_summary.radius_clamped_count == 0);
    assert(pure_summary.foreground_backdrop_driven_count == 0);
    assert(pure_summary.foreground_high_contrast_count == 0);
    assert(pure_summary.foreground_vibrant_count == 0);
    assert(pure_summary.min_foreground_contrast_ratio > 4.5f);
    assert(pure_summary.max_surface_area == 160.0f * 64.0f);
    assert(pure_summary.max_effective_radius == 12.0f);
    assert(pure_summary.max_radius_limit == 32.0f);
    assert(std::fabs(pure_summary.max_normalized_radius
                     - (12.0f / 32.0f)) < 0.0001f);
    assert(pure_summary.max_container_spacing == 0.0f);

    auto summary = diag::detail::material_runtime_summary_json(records);
    auto const& summary_obj = summary.as_object();
    assert(summary_obj.at("plan_count").as_integer() == 1);
    assert(summary_obj.at("fallback_count").as_integer() == 1);
    assert(summary_obj.at("total_runtime_passes").as_integer() == 1);
    assert(summary_obj.at("active_runtime_passes").as_integer() == 1);
    assert(summary_obj.at("backdrop_runtime_passes").as_integer() == 0);
    assert(summary_obj.at("next_frame_capture_plan_count").as_integer() == 0);
    assert(summary_obj.at("dropped_execution_stages").as_integer() == 0);
    assert(summary_obj.at("max_execution_stage_capacity").as_integer() == 4);
    assert(summary_obj.at("max_pass_texture_copy_pixels").as_integer() == 0);
    assert(summary_obj.at("total_pass_texture_copy_pixels").as_integer() == 0);
    assert(summary_obj.at("max_plan_sample_taps").as_integer() == 0);
    assert(summary_obj.at("total_plan_sample_taps").as_integer() == 0);
    assert(summary_obj.at("max_sample_taps").as_integer() == 9);
    assert(summary_obj.at("max_sampling_kernel_radius").as_integer() == 0);
    assert(summary_obj.at("containered_count").as_integer() == 0);
    assert(summary_obj.at("unioned_count").as_integer() == 0);
    assert(summary_obj.at("interactive_count").as_integer() == 0);
    assert(summary_obj.at("morph_transition_count").as_integer() == 0);
    assert(summary_obj.at("valid_shape_count").as_integer() == 1);
    assert(summary_obj.at("rounded_shape_count").as_integer() == 1);
    assert(summary_obj.at("capsule_shape_count").as_integer() == 0);
    assert(summary_obj.at("radius_clamped_count").as_integer() == 0);
    assert(summary_obj.at("foreground_backdrop_driven_count").as_integer()
           == 0);
    assert(summary_obj.at("foreground_high_contrast_count").as_integer()
           == 0);
    assert(summary_obj.at("foreground_vibrant_count").as_integer() == 0);
    assert(summary_obj.at("min_foreground_contrast_ratio").as_float()
           > 4.5f);
    assert(summary_obj.at("max_surface_area").as_float() == 160.0f * 64.0f);
    assert(summary_obj.at("max_effective_radius").as_float() == 12.0f);
    assert(summary_obj.at("max_radius_limit").as_float() == 32.0f);
    assert(std::fabs(summary_obj.at("max_normalized_radius").as_float()
                     - (12.0f / 32.0f)) < 0.0001f);
    assert(summary_obj.at("max_container_spacing").as_float() == 0.0f);

    MaterialExecutorSummary executor_summary;
    executor_summary.plan_count = 1;
    executor_summary.fallback_instance_count = 1;
    executor_summary.foreground_text_candidate_count = 2;
    executor_summary.foreground_text_remap_count = 1;
    executor_summary.cpu_decode_ns = 120;
    auto executor = diag::detail::material_executor_summary_json(
        executor_summary);
    auto const& executor_obj = executor.as_object();
    assert(executor_obj.at("plan_count").as_integer() == 1);
    assert(executor_obj.at("material_instance_count").as_integer() == 0);
    assert(executor_obj.at("fallback_instance_count").as_integer() == 1);
    assert(executor_obj.at("material_draw_calls").as_integer() == 0);
    assert(executor_obj.at("dropped_execution_stage_count").as_integer() == 0);
    assert(executor_obj.at("next_frame_capture_plan_count").as_integer() == 0);
    assert(executor_obj.at("material_max_sample_taps").as_integer() == 0);
    assert(executor_obj.at("material_total_sample_taps").as_integer() == 0);
    assert(executor_obj.at("foreground_text_candidate_count").as_integer()
           == 2);
    assert(executor_obj.at("foreground_text_remap_count").as_integer() == 1);
    assert(executor_obj.at("cpu_decode_ns").as_integer() == 120);

    auto empty = diag::detail::empty_material_renderer_contract(
        "test-semantic-fallback");
    assert(empty.at("material_pipeline_ready").as_bool() == false);
    assert(empty.at("material_backdrop_source_ready").as_bool() == false);
    assert(empty.at("material_plan_contract_version").as_integer()
           == material_plan_contract_version);
    assert(empty.at("material_plan_count").as_integer() == 0);
    assert(empty.at("material_plans").as_array().empty());
    assert(empty.at("material_runtime_summary").as_object()
               .at("plan_count").as_integer() == 0);
    assert(empty.at("material_runtime_summary").as_object()
               .at("dropped_execution_stages").as_integer() == 0);
    assert(empty.at("material_executor_summary").as_object()
               .at("plan_count").as_integer() == 0);
    assert(empty.at("material_executor_summary").as_object()
               .at("dropped_execution_stage_count").as_integer() == 0);
    assert(empty.at("material_fallback_policy").as_string()
           == "test-semantic-fallback");
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
    assert(capabilities.at("material_shader_blur").as_bool() == false);
    assert(capabilities.at("material_frame_history").as_bool() == false);

    auto runtime_file = json::parse(
        read_text_file(bundle_dir / "platform" / "wasi-runtime.json"));
    auto const& runtime_obj = runtime_file.as_object();
    assert(runtime_obj.at("host_model").as_string() == "wasi");
    assert(runtime_obj.at("frame_capture_supported").as_bool() == false);
    auto const& renderer = runtime_obj.at("renderer").as_object();
    assert(renderer.at("material_pipeline_ready").as_bool() == false);
    assert(renderer.at("material_backdrop_source_ready").as_bool() == false);
    assert(renderer.at("material_plan_contract_version").as_integer()
           == material_plan_contract_version);
    assert(renderer.at("material_plan_count").as_integer() == 0);
    assert(renderer.at("material_plans").as_array().empty());
    assert(renderer.at("material_runtime_summary").as_object()
               .at("plan_count").as_integer() == 0);
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
#if !defined(__wasi__) && !defined(__ANDROID__)
    test_paint_buffer_overflow_records_metric_and_drops_command();
    std::printf("PASS: paint overflow records metric + drops command\n");
#endif
    test_runner_records_phases();
    std::printf("PASS: runner records frame + phase histograms\n");
    test_debug_plane_semantic_tree_shape_and_stability();
    std::printf("PASS: debug plane semantic tree shape + stability\n");
    test_material_surface_semantic_debug_fields();
    std::printf("PASS: material surface semantic debug fields\n");
    test_material_app_chrome_helpers_are_semantic_materials();
    std::printf("PASS: material app chrome helpers are semantic materials\n");
    test_material_container_semantic_debug_fields();
    std::printf("PASS: material container semantic debug fields\n");
    test_overlay_semantic_debug_nodes_are_screen_fixed();
    std::printf("PASS: overlay semantic debug nodes stay screen fixed\n");
    test_material_runtime_record_json_contract();
    std::printf("PASS: material runtime record JSON contract\n");
#ifdef __wasi__
    test_wasi_debug_artifact_bundle_contract();
    std::printf("PASS: WASI debug artifact bundle contract\n");
#endif
    std::printf("\nAll diag tests passed.\n");
    return 0;
}

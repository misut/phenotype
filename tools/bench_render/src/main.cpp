// bench_render — host-only micro-benchmark for the phenotype render pipeline.
//
// Drives synthetic scenarios through the real component runner and emits a
// JSON report of diag metric deltas plus per-scenario frame timings. Used as
// the measurement baseline for the perf roadmap.
//
// Scenarios:
//   uniform_static  — N text leaves, zero churn. Expect all frames after
//                     the warm-up to hit the FNV-1a flush-skip and for
//                     layout_nodes_skipped to dominate.
//   list_churn      — N text leaves, ~churn_bp basis-points of them get a
//                     different text string each frame. Measures the
//                     diff v2 ceiling in the presence of content churn.
//   scroll_only     — N text leaves, tree unchanged; app.scroll_y bumps
//                     between frames. This is the scenario S2 (subtree
//                     paint cache) must move.

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <utility>
#include <vector>

import phenotype;

namespace {

// ============================================================
// Scenario configuration.
// ============================================================

enum class ScenarioKind { UniformStatic, ListChurn, ScrollOnly, ListReorder };

struct ScenarioConfig {
    ScenarioKind kind = ScenarioKind::UniformStatic;
    int node_count = 1000;
    int churn_bp = 0;   // basis points, 10000 = 100%
    int frames = 240;
};

ScenarioConfig g_config{};
int g_frame = 0;

// Flat column of N text leaves. list_churn varies a fraction of
// items per frame; list_reorder emits stable keys and cyclically
// rotates the visible order so the diff's keyed salvage pass is the
// only route for items to keep their layout_valid state.
void render_benchmark_tree() {
    using namespace phenotype;
    layout::column([&] {
        if (g_config.kind == ScenarioKind::ListReorder) {
            int const n = g_config.node_count;
            int const shift = g_frame % n;
            std::string label;
            for (int slot = 0; slot < n; ++slot) {
                int const i = (slot + shift) % n;
                label.assign("item-");
                label.append(std::to_string(i));
                phenotype::keyed(static_cast<std::uint32_t>(i), [&] {
                    widget::text(str{label});
                });
            }
            return;
        }
        std::string label;
        for (int i = 0; i < g_config.node_count; ++i) {
            label.assign("item-");
            label.append(std::to_string(i));
            if (g_config.kind == ScenarioKind::ListChurn) {
                int const mod = 10000;
                int const bucket = (i * 1009 + g_frame * 131) % mod;
                if (bucket < g_config.churn_bp) {
                    label.push_back('.');
                    label.append(std::to_string(g_frame));
                }
            }
            widget::text(str{label});
        }
    });
}

struct BenchApp {
    phenotype::ui::View body(phenotype::ui::Context&) const {
        return phenotype::ui::View{[] { render_benchmark_tree(); }};
    }
};

// ============================================================
// Metric snapshot — pulls the counters we care about out of AppState
// without re-parsing the serialized JSON.
// ============================================================

struct Snapshot {
    std::uint64_t rebuilds = 0;
    std::uint64_t layout_nodes_skipped = 0;
    std::uint64_t layout_nodes_computed = 0;
    std::uint64_t frames_skipped = 0;
    std::uint64_t flush_calls = 0;
    std::uint64_t alloc_nodes = 0;
    std::uint64_t arena_resets = 0;
    std::uint64_t measure_text_calls = 0;
    std::uint64_t measure_text_cache_hits = 0;
    std::uint64_t paint_subtrees_blitted = 0;
    std::uint64_t paint_bytes_blitted = 0;
    std::uint64_t scissor_emitted = 0;
    std::uint64_t keyed_lists_matched = 0;
    std::uint64_t keyed_children_matched = 0;
};

Snapshot capture() {
    namespace m = phenotype::metrics::inst;
    return Snapshot{
        .rebuilds                = m::rebuilds.total(),
        .layout_nodes_skipped    = m::layout_nodes_skipped.total(),
        .layout_nodes_computed   = m::layout_nodes_computed.total(),
        .frames_skipped          = m::frames_skipped.total(),
        .flush_calls             = m::flush_calls.total(),
        .alloc_nodes             = m::alloc_nodes.total(),
        .arena_resets            = m::arena_resets.total(),
        .measure_text_calls      = m::measure_text_calls.total(),
        .measure_text_cache_hits = m::measure_text_cache_hits.total(),
        .paint_subtrees_blitted  = m::paint_subtrees_blitted.total(),
        .paint_bytes_blitted     = m::paint_bytes_blitted.total(),
        .scissor_emitted         = m::scissor_emitted.total(),
        .keyed_lists_matched     = m::keyed_lists_matched.total(),
        .keyed_children_matched  = m::keyed_children_matched.total(),
    };
}

Snapshot delta(Snapshot const& a, Snapshot const& b) {
    return Snapshot{
        .rebuilds                = b.rebuilds - a.rebuilds,
        .layout_nodes_skipped    = b.layout_nodes_skipped - a.layout_nodes_skipped,
        .layout_nodes_computed   = b.layout_nodes_computed - a.layout_nodes_computed,
        .frames_skipped          = b.frames_skipped - a.frames_skipped,
        .flush_calls             = b.flush_calls - a.flush_calls,
        .alloc_nodes             = b.alloc_nodes - a.alloc_nodes,
        .arena_resets            = b.arena_resets - a.arena_resets,
        .measure_text_calls      = b.measure_text_calls - a.measure_text_calls,
        .measure_text_cache_hits = b.measure_text_cache_hits - a.measure_text_cache_hits,
        .paint_subtrees_blitted  = b.paint_subtrees_blitted - a.paint_subtrees_blitted,
        .paint_bytes_blitted     = b.paint_bytes_blitted - a.paint_bytes_blitted,
        .scissor_emitted         = b.scissor_emitted - a.scissor_emitted,
        .keyed_lists_matched     = b.keyed_lists_matched - a.keyed_lists_matched,
        .keyed_children_matched  = b.keyed_children_matched - a.keyed_children_matched,
    };
}

// ============================================================
// Scenario driver.
// ============================================================

struct Report {
    std::string name;
    int frames;
    int node_count;
    Snapshot measured;   // delta over measured frames (frame 0 discarded)
    std::vector<std::uint64_t> frame_times_ns;
};

struct TimingStats {
    int frames = 0;
    std::uint64_t total_ns = 0;
    std::uint64_t min_ns = 0;
    std::uint64_t max_ns = 0;
    std::uint64_t p50_ns = 0;
    std::uint64_t p95_ns = 0;
    double average_ns = 0.0;
    double average_fps_capacity = 0.0;
    double p50_fps_capacity = 0.0;
    double p95_fps_capacity = 0.0;
};

std::uint64_t percentile_from_sorted(
        std::vector<std::uint64_t> const& sorted,
        double percentile) {
    if (sorted.empty())
        return 0;
    auto const index = static_cast<std::size_t>(
        percentile * static_cast<double>(sorted.size() - 1) + 0.5);
    return sorted[std::min(index, sorted.size() - 1)];
}

double fps_capacity(std::uint64_t ns) {
    if (ns == 0)
        return 0.0;
    return 1'000'000'000.0 / static_cast<double>(ns);
}

TimingStats summarize_timings(std::vector<std::uint64_t> const& samples) {
    auto sorted = samples;
    std::ranges::sort(sorted);
    TimingStats stats;
    stats.frames = static_cast<int>(samples.size());
    stats.min_ns = sorted.empty() ? 0 : sorted.front();
    stats.max_ns = sorted.empty() ? 0 : sorted.back();
    stats.p50_ns = percentile_from_sorted(sorted, 0.50);
    stats.p95_ns = percentile_from_sorted(sorted, 0.95);
    for (auto ns : samples)
        stats.total_ns += ns;
    stats.average_ns = stats.frames > 0
        ? static_cast<double>(stats.total_ns) / static_cast<double>(stats.frames)
        : 0.0;
    stats.average_fps_capacity =
        stats.average_ns > 0.0 ? 1'000'000'000.0 / stats.average_ns : 0.0;
    stats.p50_fps_capacity = fps_capacity(stats.p50_ns);
    stats.p95_fps_capacity = fps_capacity(stats.p95_ns);
    return stats;
}

Report run_scenario(char const* name, ScenarioConfig config) {
    using namespace phenotype;
    g_config = config;

    null_host host;
    // Installs the runner and does the first trigger_rebuild — this is
    // the "warm-up" frame (cold caches, first arena growth). Discarded
    // from the measurement.
    g_frame = 0;
    phenotype::ui::run(host, BenchApp{});

    Snapshot const before = capture();
    auto frame_times = std::vector<std::uint64_t>{};
    frame_times.reserve(static_cast<std::size_t>(config.frames - 1));

    for (int f = 1; f < config.frames; ++f) {
        if (config.kind == ScenarioKind::ScrollOnly) {
            // Vary scroll_y without changing the tree. The diff will
            // match every subtree, so layout_nodes_skipped should equal
            // node_count, but today paint still re-walks and the hash
            // differs because draw_y = ay - scroll_y shifts.
            detail::g_app().scroll_y = static_cast<float>(f % 200);
        }
        g_frame = f;
        auto const start = std::chrono::steady_clock::now();
        detail::trigger_rebuild();
        auto const end = std::chrono::steady_clock::now();
        frame_times.push_back(
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    end - start).count()));
    }

    Snapshot const after = capture();
    return Report{
        .name = name,
        .frames = config.frames - 1,   // discard the warm-up frame
        .node_count = config.node_count,
        .measured = delta(before, after),
        .frame_times_ns = std::move(frame_times),
    };
}

// ============================================================
// JSON emission — manual, no dependency on jsoncpp (which would drag
// in transitive deps for a host tool).
// ============================================================

std::string json_escape(std::string const& s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += c;
                }
        }
    }
    return out;
}

void emit_snapshot(std::string& out, Snapshot const& s, char const* indent) {
    out += indent; out += "\"rebuilds\": ";                out += std::to_string(s.rebuilds);                out += ",\n";
    out += indent; out += "\"layout_nodes_skipped\": ";    out += std::to_string(s.layout_nodes_skipped);    out += ",\n";
    out += indent; out += "\"layout_nodes_computed\": ";   out += std::to_string(s.layout_nodes_computed);   out += ",\n";
    out += indent; out += "\"frames_skipped\": ";          out += std::to_string(s.frames_skipped);          out += ",\n";
    out += indent; out += "\"flush_calls\": ";             out += std::to_string(s.flush_calls);             out += ",\n";
    out += indent; out += "\"alloc_nodes\": ";             out += std::to_string(s.alloc_nodes);             out += ",\n";
    out += indent; out += "\"arena_resets\": ";            out += std::to_string(s.arena_resets);            out += ",\n";
    out += indent; out += "\"measure_text_calls\": ";      out += std::to_string(s.measure_text_calls);      out += ",\n";
    out += indent; out += "\"measure_text_cache_hits\": "; out += std::to_string(s.measure_text_cache_hits); out += ",\n";
    out += indent; out += "\"paint_subtrees_blitted\": ";  out += std::to_string(s.paint_subtrees_blitted);  out += ",\n";
    out += indent; out += "\"paint_bytes_blitted\": ";     out += std::to_string(s.paint_bytes_blitted);     out += ",\n";
    out += indent; out += "\"scissor_emitted\": ";         out += std::to_string(s.scissor_emitted);         out += ",\n";
    out += indent; out += "\"keyed_lists_matched\": ";     out += std::to_string(s.keyed_lists_matched);     out += ",\n";
    out += indent; out += "\"keyed_children_matched\": ";  out += std::to_string(s.keyed_children_matched);  out += "\n";
}

void emit_timing(std::string& out, TimingStats const& t, char const* indent) {
    out += indent; out += "\"frames\": "; out += std::to_string(t.frames); out += ",\n";
    out += indent; out += "\"total_ns\": "; out += std::to_string(t.total_ns); out += ",\n";
    out += indent; out += "\"average_ns\": "; out += std::to_string(t.average_ns); out += ",\n";
    out += indent; out += "\"min_ns\": "; out += std::to_string(t.min_ns); out += ",\n";
    out += indent; out += "\"max_ns\": "; out += std::to_string(t.max_ns); out += ",\n";
    out += indent; out += "\"p50_ns\": "; out += std::to_string(t.p50_ns); out += ",\n";
    out += indent; out += "\"p95_ns\": "; out += std::to_string(t.p95_ns); out += ",\n";
    out += indent; out += "\"average_fps_capacity\": "; out += std::to_string(t.average_fps_capacity); out += ",\n";
    out += indent; out += "\"p50_fps_capacity\": "; out += std::to_string(t.p50_fps_capacity); out += ",\n";
    out += indent; out += "\"p95_fps_capacity\": "; out += std::to_string(t.p95_fps_capacity); out += "\n";
}

std::string render_report(std::vector<Report> const& reports) {
    std::string out;
    out += "{\n  \"scenarios\": [\n";
    for (std::size_t i = 0; i < reports.size(); ++i) {
        auto const& r = reports[i];
        out += "    {\n";
        out += "      \"name\": \""; out += json_escape(r.name); out += "\",\n";
        out += "      \"frames\": "; out += std::to_string(r.frames); out += ",\n";
        out += "      \"node_count\": "; out += std::to_string(r.node_count); out += ",\n";
        out += "      \"timing\": {\n";
        emit_timing(out, summarize_timings(r.frame_times_ns), "        ");
        out += "      },\n";
        out += "      \"metrics\": {\n";
        emit_snapshot(out, r.measured, "        ");
        out += "      }\n";
        out += "    }";
        if (i + 1 < reports.size()) out += ",";
        out += "\n";
    }
    out += "  ]\n}\n";
    return out;
}

} // namespace

int main(int argc, char** argv) {
    char const* out_path = nullptr;
    for (int i = 1; i < argc; ++i) {
        std::string_view arg{argv[i]};
        if (arg == "--out" && i + 1 < argc) {
            out_path = argv[++i];
        }
    }

    std::vector<Report> reports;

    // Node counts are bounded by phenotype::Arena::MAX_NODES (4096); each
    // scenario's column has N children plus a root, so the maximum
    // practical N here is ~4000. Bumping MAX_NODES is intentionally
    // out of scope for S1 — it would affect real apps too.

    reports.push_back(run_scenario("uniform_static_1000", {
        .kind = ScenarioKind::UniformStatic,
        .node_count = 1000,
        .churn_bp = 0,
        .frames = 240,
    }));

    reports.push_back(run_scenario("uniform_static_3000", {
        .kind = ScenarioKind::UniformStatic,
        .node_count = 3000,
        .churn_bp = 0,
        .frames = 240,
    }));

    reports.push_back(run_scenario("list_churn_1000_5pct", {
        .kind = ScenarioKind::ListChurn,
        .node_count = 1000,
        .churn_bp = 500,   // 5%
        .frames = 240,
    }));

    reports.push_back(run_scenario("scroll_only_1000", {
        .kind = ScenarioKind::ScrollOnly,
        .node_count = 1000,
        .churn_bp = 0,
        .frames = 240,
    }));

    reports.push_back(run_scenario("scroll_only_3000", {
        .kind = ScenarioKind::ScrollOnly,
        .node_count = 3000,
        .churn_bp = 0,
        .frames = 240,
    }));

    reports.push_back(run_scenario("list_reorder_500", {
        .kind = ScenarioKind::ListReorder,
        .node_count = 500,
        .churn_bp = 0,
        .frames = 240,
    }));

    auto const report = render_report(reports);
    std::fputs(report.c_str(), stdout);

    if (out_path) {
        std::ofstream file(out_path);
        if (!file) {
            std::fprintf(stderr, "bench_render: failed to open %s\n", out_path);
            return 1;
        }
        file << report;
    }
    return 0;
}

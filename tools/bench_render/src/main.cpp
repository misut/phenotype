// bench_render — host-only micro-benchmark for the phenotype render pipeline.
//
// Drives three synthetic scenarios through the real phenotype::run<> runner
// and emits a JSON report of diag metric deltas plus per-scenario frame
// timings. Used as the measurement baseline for the perf roadmap that
// extends vDOM diff v2 with a paint-side subtree cache (S2) and further
// stages.
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

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <string>
#include <variant>
#include <vector>

import phenotype;

namespace {

// ============================================================
// Scenario configuration (global — phenotype::run<> resets State{}).
// ============================================================

enum class ScenarioKind { UniformStatic, ListChurn, ScrollOnly };

struct ScenarioConfig {
    ScenarioKind kind = ScenarioKind::UniformStatic;
    int node_count = 1000;
    int churn_bp = 0;   // basis points, 10000 = 100%
    int frames = 240;
};

ScenarioConfig g_config{};

// Messages: Step advances the logical frame counter used by list_churn
// to rotate which leaves carry a per-frame suffix on their text content.
struct Step {
    int frame;
};
using Msg = std::variant<Step>;

struct State {
    int frame = 0;
};

void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, Step>) state.frame = m.frame;
    }, msg);
}

// View: flat column of N text leaves. list_churn varies a fraction of
// items per frame by appending a frame-dependent suffix to force the
// diff's text-inequality branch.
void view(State const& state) {
    using namespace phenotype;
    layout::column([&] {
        std::string label;
        for (int i = 0; i < g_config.node_count; ++i) {
            label.assign("item-");
            label.append(std::to_string(i));
            if (g_config.kind == ScenarioKind::ListChurn) {
                int const mod = 10000;
                int const bucket = (i * 1009 + state.frame * 131) % mod;
                if (bucket < g_config.churn_bp) {
                    label.push_back('.');
                    label.append(std::to_string(state.frame));
                }
            }
            widget::text(str{label});
        }
    });
}

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
};

Report run_scenario(char const* name, ScenarioConfig config) {
    using namespace phenotype;
    g_config = config;

    null_host host;
    // Installs the runner and does the first trigger_rebuild — this is
    // the "warm-up" frame (cold caches, first arena growth). Discarded
    // from the measurement.
    phenotype::run<State, Msg>(host, view, update);

    Snapshot const before = capture();

    for (int f = 1; f < config.frames; ++f) {
        if (config.kind == ScenarioKind::ScrollOnly) {
            // Vary scroll_y without changing the tree. The diff will
            // match every subtree, so layout_nodes_skipped should equal
            // node_count, but today paint still re-walks and the hash
            // differs because draw_y = ay - scroll_y shifts.
            detail::g_app.scroll_y = static_cast<float>(f % 200);
        }
        detail::post<Msg>(Step{f});
        detail::trigger_rebuild();
    }

    Snapshot const after = capture();
    return Report{
        .name = name,
        .frames = config.frames - 1,   // discard the warm-up frame
        .node_count = config.node_count,
        .measured = delta(before, after),
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
    out += indent; out += "\"scissor_emitted\": ";         out += std::to_string(s.scissor_emitted);         out += "\n";
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

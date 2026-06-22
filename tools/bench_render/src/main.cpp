// bench_render — host-only micro-benchmark for the phenotype layout/scene
// pipeline. It drives synthetic View trees through the real
// phenotype::layout::LayoutScene and reports, per scenario:
//   - the draw-command counts the scene emits (panels / buttons / texts),
//   - whether those counts exceed the old fixed caps (16/128/128) that slice 10
//     replaced with storage buffers — the scene no longer truncates, so this
//     now flags scenarios that would have been clipped before the change,
//   - LayoutScene wall-clock per frame (mean / median / p95, microseconds).
//
// This is the MEASURE-FIRST baseline for Phase 5. main's LayoutScene is a pure
// function with no retained diff yet, so churn and scroll frames each pay a
// full rebuild — exactly the cost slices 10 (instancing) and 11 (damage /
// partial repaint) target. Re-run this after each perf slice and compare to
// docs/bench_baseline.json.
//
// Scenarios:
//   uniform_static — N text leaves in a scroll, identical every frame.
//   list_churn     — N text leaves; a fraction get new text content per frame.
//   scroll_only    — N text leaves, tree unchanged; the scroll offset advances.

import phenotype;
import std;

namespace ui = phenotype::ui;
namespace pl = phenotype::layout;
namespace ps = phenotype::scene;

namespace {

// A deterministic measure stub: width proportional to length, fixed line
// height. The benchmark measures layout cost, not text shaping, so a closed
// form keeps frames comparable across runs and machines.
ui::Size MeasureStub(std::string_view content, float font_size, float) {
  return {static_cast<float>(content.size()) * font_size * 0.5f, font_size};
}

enum class ScenarioKind { UniformStatic, ListChurn, ScrollOnly, FlatDense };

struct ScenarioConfig {
  std::string name;
  ScenarioKind kind;
  int node_count;
  int churn_percent; // fraction of leaves that change text each frame
  int frames;
};

// Build the scenario's View tree for a given frame. Frame-dependent state
// (churned text, scroll offset) is baked into the tree, mirroring how a real
// app rebuilds its body() from changed state each frame.
ui::View BuildTree(const ScenarioConfig &config, int frame) {
  std::vector<ui::View> rows;
  rows.reserve(static_cast<std::size_t>(config.node_count));
  int churn_stride = config.churn_percent > 0 ? std::max(1, 100 / config.churn_percent) : 0;
  for (int index = 0; index < config.node_count; ++index) {
    std::string label = "item-" + std::to_string(index);
    if (churn_stride > 0 && (index % churn_stride) == 0) {
      label += "-" + std::to_string(frame);
    }
    // FlatDense uses short rows so more than the 128-text cap fit the viewport
    // at once (the cap only truncates when many items are visible without
    // scroll culling); the other scenarios use normal-height rows.
    float row_height = config.kind == ScenarioKind::FlatDense ? 3.0f : 18.0f;
    rows.push_back(ui::text(label).size({120.0f, row_height}));
  }

  ui::View column;
  column.kind = ui::ViewKind::stack;
  column.axis = ui::LayoutAxis::vertical;
  column.children = std::move(rows);
  column.child_spacing = 2.0f;

  // FlatDense lays the whole stack out with no scroll viewport, so every leaf
  // reaches the scene and the per-kind text cap (128) truncates — the limit
  // slice 10 removes. The other scenarios wrap the column in a scroll, whose
  // culling keeps only the visible window.
  if (config.kind == ScenarioKind::FlatDense) {
    return std::move(column).expand();
  }

  ui::View scroll = ui::layout::scroll(std::move(column));
  if (config.kind == ScenarioKind::ScrollOnly) {
    scroll = std::move(scroll).scroll_offset(static_cast<float>(frame) * 8.0f);
  }
  return scroll;
}

struct ScenarioResult {
  std::string name;
  std::size_t panels = 0;
  std::size_t buttons = 0;
  std::size_t texts = 0;
  bool exceeds_old_caps = false;
  double mean_us = 0.0;
  double median_us = 0.0;
  double p95_us = 0.0;
};

ScenarioResult RunScenario(const ScenarioConfig &config) {
  constexpr float kWidth = 480.0f;
  constexpr float kHeight = 720.0f;

  std::vector<double> frame_us;
  frame_us.reserve(static_cast<std::size_t>(config.frames));
  ps::SceneLayout last;

  for (int frame = 0; frame < config.frames; ++frame) {
    ui::View tree = BuildTree(config, frame);
    ps::LayoutContext context;
    auto start = std::chrono::steady_clock::now();
    ps::SceneLayout scene = pl::LayoutScene(MeasureStub, tree, kWidth, kHeight, context);
    auto end = std::chrono::steady_clock::now();
    frame_us.push_back(std::chrono::duration<double, std::micro>(end - start).count());
    last = std::move(scene);
  }

  ScenarioResult result;
  result.name = config.name;
  result.panels = last.background.panels.size() + last.foreground.panels.size();
  result.buttons = last.background.buttons.size() + last.foreground.buttons.size();
  result.texts = last.background.texts.size() + last.foreground.texts.size();
  // Since the storage-buffer slice the scene no longer truncates — every
  // visible record is emitted. This flag now reports whether the emitted counts
  // exceed the old fixed caps (now reserve hints), i.e. whether this scenario
  // would have been silently clipped before the change.
  result.exceeds_old_caps = result.texts > ps::kTextReserve || result.panels > ps::kPanelReserve ||
                            result.buttons > ps::kSymbolButtonReserve;

  std::sort(frame_us.begin(), frame_us.end());
  double sum = 0.0;
  for (double value : frame_us) {
    sum += value;
  }
  if (!frame_us.empty()) {
    result.mean_us = sum / static_cast<double>(frame_us.size());
    result.median_us = frame_us[frame_us.size() / 2];
    result.p95_us = frame_us[static_cast<std::size_t>(static_cast<double>(frame_us.size()) * 0.95)];
  }
  return result;
}

void PrintJson(const std::vector<ScenarioResult> &results) {
  std::println("{{");
  std::println("  \"pipeline\": \"layout-scene\",");
  std::println("  \"scenarios\": [");
  for (std::size_t index = 0; index < results.size(); ++index) {
    const ScenarioResult &r = results[index];
    std::println("    {{");
    std::println("      \"name\": \"{}\",", r.name);
    std::println("      \"emitted\": {{ \"panels\": {}, \"buttons\": {}, \"texts\": {} }},",
        r.panels, r.buttons, r.texts);
    std::println("      \"exceeds_old_caps\": {},", r.exceeds_old_caps ? "true" : "false");
    std::println("      \"layout_us\": {{ \"mean\": {:.2f}, \"median\": {:.2f}, \"p95\": {:.2f} }}",
        r.mean_us, r.median_us, r.p95_us);
    std::println("    }}{}", index + 1 == results.size() ? "" : ",");
  }
  std::println("  ]");
  std::println("}}");
}

} // namespace

int main() {
  const std::vector<ScenarioConfig> configs{
      {"uniform_static", ScenarioKind::UniformStatic, 1000, 0, 240},
      {"list_churn", ScenarioKind::ListChurn, 1000, 5, 240},
      {"scroll_only", ScenarioKind::ScrollOnly, 1000, 0, 240},
      {"flat_dense", ScenarioKind::FlatDense, 1000, 0, 240},
  };

  std::vector<ScenarioResult> results;
  results.reserve(configs.size());
  for (const ScenarioConfig &config : configs) {
    results.push_back(RunScenario(config));
  }

  PrintJson(results);
  return 0;
}

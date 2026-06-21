import phenotype;
import std;

namespace ui = phenotype::ui;
namespace widget = phenotype::widget;
namespace ps = phenotype::scene;

namespace {

bool Approx(float lhs, float rhs) noexcept { return std::abs(lhs - rhs) < 0.01f; }

// A measure stub: width proportional to length, fixed line height. Enough to
// drive LayoutScene without a platform text engine.
ui::Size MeasureStub(std::string_view content, float font_size, float) {
  return {static_cast<float>(content.size()) * font_size * 0.5f, font_size};
}

} // namespace

// Covers the token vocabulary and the component builders: each must resolve to
// the plain View fields the layout engine consumes, and composed widgets must
// remain renderable on the kinds main already supports.
int main() {
  // --- Spacing / color / font tokens --------------------------------------
  {
    if (!Approx(ui::Space(ui::SpaceToken::none), 0.0f) ||
        !Approx(ui::Space(ui::SpaceToken::md), 12.0f) ||
        !Approx(ui::Space(ui::SpaceToken::xl), 32.0f)) {
      return 1;
    }
    ui::Insets insets = ui::SpaceInsets(ui::SpaceToken::sm);
    if (!Approx(insets.left, 8.0f) || !Approx(insets.bottom, 8.0f)) {
      return 2;
    }
    if (ui::Resolve(ui::TextColor::primary).red != ui::primary_label().red ||
        ui::Resolve(ui::TextColor::muted).red != ui::disabled_label().red) {
      return 3;
    }
    ui::FontMetrics title = ui::Resolve(ui::Font::title);
    if (!Approx(title.size, 28.0f) || !Approx(title.weight, 600.0f)) {
      return 4;
    }
  }

  // --- Text builder maps font/color tokens onto View fields ---------------
  {
    ui::View plain = ui::Text("hi");
    if (plain.kind != ui::ViewKind::text || plain.text_content != "hi") {
      return 5;
    }
    ui::View titled = ui::Text("Title", ui::Font::title, ui::TextColor::muted);
    if (!Approx(titled.font_size_value, 28.0f) || !Approx(titled.font_weight_value, 600.0f) ||
        titled.foreground_color.red != ui::disabled_label().red) {
      return 6;
    }
    // Existing text modifiers still chain off the builder result.
    ui::View clamped = ui::Text("body", ui::Font::body).line_limit(1).center_text();
    if (clamped.text_line_limit != 1 || !clamped.centers_text) {
      return 7;
    }
  }

  // --- Stacks apply spacing rung + centering ------------------------------
  {
    ui::View column = ui::VStack(ui::StackOptions{.spacing = ui::SpaceToken::lg,
                                     .centers_children = true},
        ui::Text("a"), ui::Text("b"));
    if (column.kind != ui::ViewKind::stack || column.axis != ui::LayoutAxis::vertical ||
        !Approx(column.child_spacing, 20.0f) || !column.centers_children ||
        column.children.size() != 2) {
      return 8;
    }
    ui::View row = ui::HStack(ui::Text("x"), ui::Text("y"), ui::Text("z"));
    if (row.kind != ui::ViewKind::stack || row.axis != ui::LayoutAxis::horizontal ||
        row.children.size() != 3) {
      return 9;
    }
  }

  // --- Card / Box overlay a panel under content ---------------------------
  {
    ui::View card = ui::Card(ui::Text("inside"));
    if (card.kind != ui::ViewKind::stack || card.axis != ui::LayoutAxis::overlay ||
        card.children.size() != 2 || card.children[0].kind != ui::ViewKind::panel ||
        !Approx(card.children[0].corner_radius_value, ui::default_surface_corner_radius()) ||
        card.children[1].kind != ui::ViewKind::text) {
      return 10;
    }
  }

  // --- Button composes a clickable labelled panel -------------------------
  {
    bool clicked = false;
    ui::View btn = ui::Button("Tap", [&clicked] { clicked = true; }, ui::ButtonRole::normal);
    if (btn.kind != ui::ViewKind::stack || btn.axis != ui::LayoutAxis::overlay ||
        !btn.click_action || btn.children.size() != 2 ||
        btn.children[0].kind != ui::ViewKind::panel ||
        btn.children[1].kind != ui::ViewKind::text || !btn.children[1].centers_text) {
      return 11;
    }
    btn.click_action();
    if (!clicked) {
      return 12;
    }
  }

  // --- The composed button is actually renderable: it lays out into a hit
  //     target and a text run on today's layout engine ----------------------
  {
    bool fired = false;
    ui::View root = ui::VStack(ui::StackOptions{.spacing = ui::SpaceToken::md},
        ui::Text("Counter", ui::Font::title),
        ui::Button("Increment", [&fired] { fired = true; }));

    ps::LayoutContext context;
    ps::SceneLayout laid = phenotype::layout::LayoutScene(MeasureStub, root, 400.0f, 300.0f, context);
    // Title + button label both produce text runs; the button produces a hit
    // target carrying its action.
    if (laid.background.texts.size() < 2) {
      return 13;
    }
    if (laid.hit_targets.empty()) {
      return 14;
    }
    // Fire the laid-out hit target and confirm it routes to our callback.
    laid.hit_targets.front().action();
    if (!fired) {
      return 15;
    }
  }

  // --- widget:: callback wrappers reach the same builders -----------------
  {
    bool clicked = false;
    ui::View w = widget::button("Go", [&clicked] { clicked = true; });
    if (!w.click_action) {
      return 16;
    }
    w.click_action();
    if (!clicked) {
      return 17;
    }
    ui::View l = widget::label("note");
    if (l.kind != ui::ViewKind::text || l.text_content != "note") {
      return 18;
    }
  }

  // --- .key() round-trips a stable identity onto a View -------------------
  {
    ui::View tagged = ui::Text("row").key(42ull);
    if (tagged.view_key != 42ull) {
      return 19;
    }
    // Default is "no key".
    if (ui::Text("plain").view_key != 0ull) {
      return 20;
    }
  }

  // --- ForEach builds one keyed child per item ----------------------------
  {
    std::vector<int> ids{10, 20, 30};
    ui::View list = ui::ForEach(
        ids, [](int id) { return id; },
        [](int id) { return ui::Text(std::to_string(id)); });
    if (list.kind != ui::ViewKind::stack || list.children.size() != 3) {
      return 21;
    }
    // Each child carries a distinct, non-zero key derived from its id.
    if (list.children[0].view_key == 0ull ||
        list.children[0].view_key == list.children[1].view_key ||
        list.children[1].view_key == list.children[2].view_key) {
      return 22;
    }
    // item_fn output is preserved (text content), only the key is stamped.
    if (list.children[1].text_content != "20") {
      return 23;
    }
  }

  // --- ForEach over an empty range yields an empty stack ------------------
  {
    std::vector<int> none;
    ui::View list = ui::ForEach(
        none, [](int id) { return id; },
        [](int id) { return ui::Text(std::to_string(id)); });
    if (list.kind != ui::ViewKind::stack || !list.children.empty()) {
      return 24;
    }
  }

  // --- ForEach accepts a verbatim uint64 key and string keys --------------
  {
    std::vector<std::string> names{"alpha", "beta"};
    ui::View list = ui::ForEach(
        names, [](const std::string &name) { return std::string_view{name}; },
        [](const std::string &name) { return ui::Text(name); });
    if (list.children.size() != 2 ||
        list.children[0].view_key == list.children[1].view_key) {
      return 25;
    }
    // A key_fn that returns uint64 is used verbatim (non-zero preserved).
    std::vector<int> ids{7};
    ui::View verbatim = ui::ForEach(
        ids, [](int) { return std::uint64_t{999}; },
        [](int id) { return ui::Text(std::to_string(id)); });
    if (verbatim.children.front().view_key != 999ull) {
      return 26;
    }
  }

  return 0;
}

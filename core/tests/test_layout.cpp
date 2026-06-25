import phenotype;
import std;

namespace ui = phenotype::ui;
namespace pl = phenotype::layout;

namespace {

// Deterministic text metrics so layout math is reproducible without a platform
// text engine: width = glyph_count * font_size * 0.5, height = font_size.
pl::MeasureTextFn StubMeasure() {
  return [](std::string_view content, float font_size, float /*font_weight*/) -> ui::Size {
    return {static_cast<float>(content.size()) * font_size * 0.5f, font_size};
  };
}

bool Approx(float lhs, float rhs) noexcept { return std::abs(lhs - rhs) < 0.01f; }

bool ApproxRect(pl::LayoutRect rect, float x, float y, float width, float height) noexcept {
  return Approx(rect.x, x) && Approx(rect.y, y) && Approx(rect.width, width) &&
         Approx(rect.height, height);
}

} // namespace

int main() {
  pl::MeasureTextFn measure = StubMeasure();

  // --- Test 1: vertical stack flex distribution -----------------------------
  // Fixed 20 + flexible + fixed 30 with spacing 10 in a 100-tall box:
  // fixed_main = 20 + 30 + 10*2 = 70, flexible = (100 - 70) / 1 = 30.
  {
    ui::View root = ui::layout::vstack([](ui::Block &body) {
      body << ui::panel(ui::white()).size({0.0f, 20.0f}).expand_width();
      body << ui::panel(ui::white()).expand();
      body << ui::panel(ui::white()).size({0.0f, 30.0f}).expand_width();
    }).spacing(10.0f);

    pl::SceneLayout scene = pl::LayoutScene(measure, root, 100.0f, 100.0f, {});
    if (scene.background.panels.size() != 3) {
      return 1;
    }
    if (!ApproxRect(scene.background.panels[0].frame, 0.0f, 0.0f, 100.0f, 20.0f)) {
      return 2;
    }
    if (!ApproxRect(scene.background.panels[1].frame, 0.0f, 30.0f, 100.0f, 30.0f)) {
      return 3;
    }
    if (!ApproxRect(scene.background.panels[2].frame, 0.0f, 70.0f, 100.0f, 30.0f)) {
      return 4;
    }
  }

  // --- Test 2: grid natural-height math -------------------------------------
  // 7 items, min column width 100 in 300 available -> 3 columns -> 3 rows.
  // height = row_height(50) * 3 = 150.
  {
    ui::View grid = ui::layout::grid([](ui::Block &cells) {
      for (int index = 0; index < 7; ++index) {
        cells << ui::text("x");
      }
    }).grid_metrics(100.0f, 50.0f, 0.0f, 0.0f);

    ui::Size natural = pl::NaturalContentSize(measure, grid, 300.0f);
    if (!Approx(natural.width, 300.0f) || !Approx(natural.height, 150.0f)) {
      return 5;
    }
  }

  // --- Test 3: scroll culling visible range ---------------------------------
  // 6 rows of height 50 in a single column, viewport 100 tall. Only rows whose
  // frame intersects the clip render: rows at y=0 and y=50; y=100 has zero
  // overlap. Expect exactly 2 rendered texts despite 6 children.
  {
    ui::View scroller = ui::layout::scroll(ui::layout::grid([](ui::Block &cells) {
      for (int index = 0; index < 6; ++index) {
        cells << ui::text("row");
      }
    }).grid_metrics(1000.0f, 50.0f, 0.0f, 0.0f));

    pl::SceneLayout scene = pl::LayoutScene(measure, scroller, 200.0f, 100.0f, {});
    if (scene.background.texts.size() != 2) {
      return 6;
    }
    if (!ApproxRect(scene.background.texts[0].frame, 0.0f, 0.0f, 200.0f, 50.0f)) {
      return 7;
    }
    if (!ApproxRect(scene.background.texts[1].frame, 0.0f, 50.0f, 200.0f, 50.0f)) {
      return 8;
    }
  }

  // --- Test 4: button_group divider placement -------------------------------
  // First visible button draws the group control + divider; the rest do not.
  {
    ui::View group = ui::button_group(
        ui::button(ui::icon(ui::Symbol::chevron_left)),
        ui::button(ui::icon(ui::Symbol::chevron_right)));

    pl::SceneLayout scene = pl::LayoutScene(measure, group, 200.0f, 100.0f, {});
    if (scene.background.buttons.size() != 2) {
      return 9;
    }
    if (!scene.background.buttons[0].draws_control || !scene.background.buttons[0].draws_divider) {
      return 10;
    }
    if (scene.background.buttons[1].draws_control || scene.background.buttons[1].draws_divider) {
      return 11;
    }
    if (scene.background.buttons[0].symbol != ui::Symbol::chevron_left ||
        scene.background.buttons[1].symbol != ui::Symbol::chevron_right) {
      return 12;
    }
  }

  // --- Test 5: leading window controls offset -------------------------------
  // A view anchored after leading controls {x:0,w:60} with spacing 12 should
  // start at x = 60 + 12 = 72.
  {
    pl::LayoutContext context;
    context.window_controls.has_leading_controls = true;
    context.window_controls.leading_controls = {0.0f, 0.0f, 60.0f, 20.0f};

    ui::View anchored =
        ui::panel(ui::white()).size({50.0f, 20.0f}).after_leading_window_controls(12.0f);

    pl::SceneLayout scene = pl::LayoutScene(measure, anchored, 200.0f, 100.0f, context);
    if (scene.background.panels.size() != 1) {
      return 13;
    }
    if (!Approx(scene.background.panels[0].frame.x, 72.0f)) {
      return 14;
    }
  }

  // --- Test 6: visual effect panel routes later draws to the foreground -----
  // A visual_effect_panel records an effect and flips subsequent draws to the
  // foreground layer (so they composite over the blur).
  {
    ui::View root = ui::layout::zstack([](ui::Block &surface) {
      surface << ui::visual_effect_panel(ui::control_background()).expand();
      surface << ui::text("over").size({40.0f, 18.0f});
    });

    pl::SceneLayout scene = pl::LayoutScene(measure, root, 200.0f, 100.0f, {});
    if (scene.effects.size() != 1 || !scene.uses_foreground_layer) {
      return 15;
    }
    if (scene.foreground.texts.size() != 1 || !scene.background.texts.empty()) {
      return 16;
    }
  }

  // --- Test 7: hit targets collect click actions with enabled state ---------
  {
    bool clicked = false;
    ui::View root = ui::layout::vstack([&clicked](ui::Block &body) {
      body << ui::panel(ui::white())
                  .size({40.0f, 20.0f})
                  .expand_width()
                  .on_click([&clicked] { clicked = true; });
    });

    pl::SceneLayout scene = pl::LayoutScene(measure, root, 100.0f, 100.0f, {});
    if (scene.hit_targets.size() != 1 || !scene.hit_targets[0].is_enabled) {
      return 17;
    }
    scene.hit_targets[0].action();
    if (!clicked) {
      return 18;
    }
  }

  // --- Test 8: material kind drives effect-panel blur amount + tint ----------
  // A thicker material frosts more (higher blur_amount) and tints harder
  // (higher color alpha) than a thinner one; the resolvers are monotonic.
  {
    ui::View thin = ui::layout::zstack([](ui::Block &surface) {
      surface << ui::visual_effect_panel().material_kind(ui::MaterialKind::thin).expand();
    });
    ui::View thick = ui::layout::zstack([](ui::Block &surface) {
      surface << ui::visual_effect_panel().material_kind(ui::MaterialKind::thick).expand();
    });
    pl::SceneLayout thin_scene = pl::LayoutScene(measure, thin, 200.0f, 100.0f, {});
    pl::SceneLayout thick_scene = pl::LayoutScene(measure, thick, 200.0f, 100.0f, {});
    if (thin_scene.effects.size() != 1 || thick_scene.effects.size() != 1) {
      return 19;
    }
    if (!(thick_scene.effects[0].blur_amount > thin_scene.effects[0].blur_amount)) {
      return 20;
    }
    if (!(thick_scene.effects[0].color.alpha > thin_scene.effects[0].color.alpha)) {
      return 21;
    }
    // The resolvers themselves are ordered across the ramp.
    if (!(ui::MaterialBlurAmount(ui::MaterialKind::clear) <
              ui::MaterialBlurAmount(ui::MaterialKind::regular) &&
            ui::MaterialBlurAmount(ui::MaterialKind::regular) <=
                ui::MaterialBlurAmount(ui::MaterialKind::thick))) {
      return 22;
    }
  }

  // --- Test 9: leading-window-controls offset shrinks width, keeps trailing
  //     content on screen. Regression: after_leading_window_controls shifted
  //     rect.x without reducing width, so a spacer-pushed trailing button
  //     overflowed past the window edge and got culled (the missing Files
  //     search button).
  {
    bool fired = false;
    ui::View bar = ui::layout::hstack([&fired](ui::Block &b) {
      b << ui::text("title").size({40.0f, 20.0f});
      b << ui::spacer();
      b << ui::panel(ui::white()).size({40.0f, 36.0f}).on_click([&fired] { fired = true; });
    })
                       .after_leading_window_controls(12.0f)
                       .size({0.0f, 52.0f})
                       .expand_width();

    pl::LayoutContext context;
    context.window_controls.has_leading_controls = true;
    context.window_controls.leading_controls = {20.0f, 6.0f, 60.0f, 20.0f};

    pl::SceneLayout scene = pl::LayoutScene(measure, bar, 400.0f, 52.0f, context);
    // The trailing panel must still be emitted and sit within the 400px window.
    if (scene.background.panels.size() != 1) {
      return 23;
    }
    const pl::PanelLayout &trailing = scene.background.panels.front();
    if (trailing.frame.x + trailing.frame.width > 400.0f + 0.5f) {
      return 24;
    }
    // And its hit target must be reachable (not culled off-screen).
    if (scene.hit_targets.empty()) {
      return 25;
    }
  }

  // --- Test: panel shadow flows from View into the scene record -------------
  {
    ui::Shadow shadow{{0.0f, 0.0f, 0.0f, 0.5f}, 12.0f, {0.0f, 4.0f}};
    ui::View root = ui::panel(ui::white()).expand().shadow(shadow);

    pl::SceneLayout scene = pl::LayoutScene(measure, root, 100.0f, 100.0f, {});
    if (scene.background.panels.size() != 1) {
      return 26;
    }
    const pl::PanelLayout &panel = scene.background.panels.front();
    if (!Approx(panel.shadow.color.alpha, 0.5f) || !Approx(panel.shadow.blur_radius, 12.0f) ||
        !Approx(panel.shadow.offset.height, 4.0f)) {
      return 27;
    }
  }

  // --- Test: content_surface_panel carries a default (non-empty) shadow -----
  {
    ui::View surface = ui::content_surface_panel().expand();
    pl::SceneLayout scene = pl::LayoutScene(measure, surface, 100.0f, 100.0f, {});
    if (scene.background.panels.size() != 1) {
      return 28;
    }
    // A visible shadow means a positive alpha and blur; a bare panel() has neither.
    const pl::PanelLayout &panel = scene.background.panels.front();
    if (panel.shadow.color.alpha <= 0.0f || panel.shadow.blur_radius <= 0.0f) {
      return 29;
    }
  }

  return 0;
}

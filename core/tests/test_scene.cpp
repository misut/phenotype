import phenotype;
import std;

namespace ui = phenotype::ui;
namespace ps = phenotype::scene;

namespace {

bool Approx(float lhs, float rhs) noexcept { return std::abs(lhs - rhs) < 0.01f; }

bool ApproxRect(ps::LayoutRect rect, float x, float y, float width, float height) noexcept {
  return Approx(rect.x, x) && Approx(rect.y, y) && Approx(rect.width, width) &&
         Approx(rect.height, height);
}

} // namespace

// Direct coverage of the platform-neutral scene geometry helpers. These are the
// contract that backends (Dawn on macOS, GDI on Windows) build on, so they are
// tested independently of the layout traversal that normally feeds them.
int main() {
  // --- Intersect: overlapping and disjoint --------------------------------
  {
    ps::LayoutRect a{0.0f, 0.0f, 100.0f, 100.0f};
    ps::LayoutRect b{50.0f, 50.0f, 100.0f, 100.0f};
    if (!ApproxRect(ps::Intersect(a, b), 50.0f, 50.0f, 50.0f, 50.0f)) {
      return 1;
    }
    ps::LayoutRect c{200.0f, 200.0f, 10.0f, 10.0f};
    if (ps::HasArea(ps::Intersect(a, c))) {
      return 2;
    }
  }

  // --- Union --------------------------------------------------------------
  {
    ps::LayoutRect a{0.0f, 0.0f, 20.0f, 20.0f};
    ps::LayoutRect b{30.0f, 30.0f, 10.0f, 10.0f};
    if (!ApproxRect(ps::Union(a, b), 0.0f, 0.0f, 40.0f, 40.0f)) {
      return 3;
    }
  }

  // --- IsVisibleInClip ----------------------------------------------------
  {
    ps::LayoutRect rect{10.0f, 10.0f, 20.0f, 20.0f};
    if (!ps::IsVisibleInClip(rect, std::nullopt)) {
      return 4;
    }
    if (!ps::IsVisibleInClip(rect, ps::LayoutRect{0.0f, 0.0f, 15.0f, 15.0f})) {
      return 5;
    }
    if (ps::IsVisibleInClip(rect, ps::LayoutRect{100.0f, 100.0f, 5.0f, 5.0f})) {
      return 6;
    }
  }

  // --- EffectBounds: clipped union of effect panels -----------------------
  {
    std::vector<ps::EffectPanelLayout> effects;
    effects.push_back({.frame = {0.0f, 0.0f, 50.0f, 50.0f}});
    effects.push_back({.frame = {100.0f, 0.0f, 50.0f, 50.0f},
        .clip_rect = ps::LayoutRect{110.0f, 0.0f, 10.0f, 50.0f}});
    std::optional<ps::LayoutRect> bounds = ps::EffectBounds(effects);
    if (!bounds.has_value()) {
      return 7;
    }
    // Union of {0,0,50,50} and the clipped {110,0,10,50} -> {0,0,120,50}.
    if (!ApproxRect(*bounds, 0.0f, 0.0f, 120.0f, 50.0f)) {
      return 8;
    }
    // EffectBounds of an empty list must be nullopt.
    if (ps::EffectBounds({}).has_value()) {
      return 9;
    }
  }

  // --- Contains (hit testing point in device points) ----------------------
  {
    ps::LayoutRect rect{10.0f, 10.0f, 30.0f, 30.0f};
    if (!ps::Contains(rect, ui::Size{25.0f, 25.0f})) {
      return 10;
    }
    if (ps::Contains(rect, ui::Size{5.0f, 25.0f})) {
      return 11;
    }
  }

  // --- Enum mappings used for uniform packing -----------------------------
  {
    if (!Approx(ps::ControlShapeValue(ui::ControlShape::square_circle), 0.0f) ||
        !Approx(ps::ControlShapeValue(ui::ControlShape::capsule), 1.0f)) {
      return 12;
    }
    if (!Approx(ps::CornerMode(false, false), 0.0f) ||
        !Approx(ps::CornerMode(true, false), 1.0f) ||
        !Approx(ps::CornerMode(false, true), 2.0f)) {
      return 13;
    }
  }

  return 0;
}

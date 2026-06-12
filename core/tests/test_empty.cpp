import phenotype;

int main() {
  namespace ui = phenotype::ui;

  constexpr phenotype::MaterialSymbolOptions default_options;
  static_assert(!default_options.fill);
  static_assert(default_options.weight == 400.0f);
  static_assert(default_options.grade == 0.0f);
  static_assert(default_options.optical_size == 24.0f);

  constexpr auto left = phenotype::MaterialSymbolChevronGeometryFor(
      phenotype::MaterialSymbolIcon::chevron_left);
  constexpr auto right = phenotype::MaterialSymbolChevronGeometryFor(
      phenotype::MaterialSymbolIcon::chevron_right);
  static_assert(left.outer_x == -right.outer_x);
  static_assert(left.center_x == -right.center_x);
  static_assert(left.top_y == right.top_y);
  static_assert(left.bottom_y == right.bottom_y);

  auto toolbar =
      ui::layout::hstack(ui::button(ui::icon(ui::Symbol::chevron_left))
                             .role(ui::ButtonRole::back)
                             .accessibility_label("Back"),
                         ui::button(ui::icon(ui::Symbol::chevron_right))
                             .role(ui::ButtonRole::forward)
                             .accessibility_label("Forward"))
          .spacing(8.0f)
          .padding({.left = 48.0f, .top = 36.0f});

  if (toolbar.kind != ui::ViewKind::stack ||
      toolbar.axis != ui::LayoutAxis::horizontal) {
    return 1;
  }
  if (toolbar.children.size() != 2 || toolbar.child_spacing != 8.0f ||
      toolbar.content_padding.left != 48.0f ||
      toolbar.content_padding.top != 36.0f) {
    return 2;
  }
  if (toolbar.children[0].button_role != ui::ButtonRole::back ||
      toolbar.children[1].button_role != ui::ButtonRole::forward) {
    return 3;
  }
  if (toolbar.children[0].children[0].symbol != ui::Symbol::chevron_left ||
      toolbar.children[1].children[0].symbol != ui::Symbol::chevron_right) {
    return 4;
  }
  if (toolbar.children[0].accessibility_label_text != "Back" ||
      toolbar.children[1].accessibility_label_text != "Forward") {
    return 5;
  }

  return 0;
}

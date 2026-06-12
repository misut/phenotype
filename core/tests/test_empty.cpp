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

  auto root = ui::layout::vstack([](ui::Block &body) {
    body << ui::button_group([](ui::Block &group) {
              group << ui::button(ui::icon(ui::Symbol::chevron_left))
                           .role(ui::ButtonRole::back)
                           .accessibility_label("Back");
              group << ui::button(ui::icon(ui::Symbol::chevron_right))
                           .role(ui::ButtonRole::forward)
                           .accessibility_label("Forward");
            })
                .shape(ui::ControlShape::capsule)
                .after_leading_window_controls(12.0f);
    body << ui::spacer();
  });

  if (root.kind != ui::ViewKind::stack ||
      root.axis != ui::LayoutAxis::vertical || root.children.size() != 2 ||
      root.children[1].kind != ui::ViewKind::spacer) {
    return 1;
  }

  const ui::View &toolbar = root.children[0];

  if (toolbar.kind != ui::ViewKind::button_group ||
      toolbar.axis != ui::LayoutAxis::horizontal) {
    return 2;
  }
  if (toolbar.children.size() != 2 ||
      toolbar.control_shape != ui::ControlShape::capsule ||
      !toolbar.leading_window_controls_placement.is_enabled ||
      toolbar.leading_window_controls_placement.spacing != 12.0f ||
      !toolbar.leading_window_controls_placement.aligns_vertical_center) {
    return 3;
  }
  if (toolbar.children[0].button_role != ui::ButtonRole::back ||
      toolbar.children[1].button_role != ui::ButtonRole::forward) {
    return 4;
  }
  if (toolbar.children[0].children[0].symbol != ui::Symbol::chevron_left ||
      toolbar.children[1].children[0].symbol != ui::Symbol::chevron_right) {
    return 5;
  }
  if (toolbar.children[0].accessibility_label_text != "Back" ||
      toolbar.children[1].accessibility_label_text != "Forward") {
    return 6;
  }

  auto direct_group =
      ui::button_group(ui::button(ui::icon(ui::Symbol::chevron_left)),
                       ui::button(ui::icon(ui::Symbol::chevron_right)));
  if (direct_group.kind != ui::ViewKind::button_group ||
      direct_group.children.size() != 2) {
    return 7;
  }

  return 0;
}

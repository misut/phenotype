import phenotype;

int main() {
  namespace ui = phenotype::ui;

  constexpr phenotype::MaterialSymbolOptions default_options;
  static_assert(!default_options.fill);
  static_assert(default_options.weight == 400.0f);
  static_assert(default_options.grade == 0.0f);
  static_assert(default_options.optical_size == 24.0f);

  auto root = ui::layout::vstack([](ui::Block &body) {
    body << ui::layout::hstack([](ui::Block &toolbar) {
              toolbar << ui::button_group([](ui::Block &group) {
                group << ui::button(ui::icon(ui::Symbol::chevron_left))
                             .role(ui::ButtonRole::back)
                             .accessibility_label("Back");
                group << ui::button(ui::icon(ui::Symbol::chevron_right))
                             .role(ui::ButtonRole::forward)
                             .accessibility_label("Forward");
              }).shape(ui::ControlShape::capsule);
              toolbar << ui::text("kakao")
                             .font_size(20.0f)
                             .font_weight(550.0f)
                             .foreground(ui::primary_label());
            })
                .spacing(24.0f)
                .after_leading_window_controls(12.0f);
    body << ui::layout::zstack([](ui::Block &surface) {
      surface << ui::panel(ui::control_background()).corner_radius(18.0f)
                     .expand();
      surface << ui::layout::grid([](ui::Block &grid) {
                   grid << ui::layout::vstack([](ui::Block &tile) {
                     tile << ui::icon(ui::Symbol::folder,
                                      {.fill = true,
                                       .weight = 400.0f,
                                       .grade = 0.0f,
                                       .optical_size = 42.0f});
                     tile << ui::text("Documents")
                                 .font_size(13.0f)
                                 .font_weight(450.0f)
                                 .center_text()
                                 .line_limit(1)
                                 .overflow(ui::TextOverflow::ellipsis)
                                 .truncation(ui::TextTruncation::tail)
                                 .size({96.0f, 18.0f});
                   }).spacing(8.0f)
                       .center_children();
                   grid << ui::layout::vstack([](ui::Block &tile) {
                     tile << ui::icon(ui::Symbol::description,
                                      {.fill = false,
                                       .weight = 400.0f,
                                       .grade = 0.0f,
                                       .optical_size = 42.0f});
                     tile << ui::text("Notes.txt")
                                 .font_size(13.0f)
                                 .font_weight(450.0f)
                                 .center_text()
                                 .line_limit(1)
                                 .overflow(ui::TextOverflow::ellipsis)
                                 .truncation(ui::TextTruncation::tail)
                                 .size({96.0f, 18.0f});
                   }).spacing(8.0f)
                       .center_children();
                 })
                     .grid_metrics(112.0f, 92.0f, 18.0f, 20.0f)
                     .expand();
    }).expand();
  }).spacing(20.0f)
      .padding({24.0f, 0.0f, 24.0f, 24.0f});

  if (root.kind != ui::ViewKind::stack ||
      root.axis != ui::LayoutAxis::vertical || root.children.size() != 2 ||
      root.children[1].kind != ui::ViewKind::stack ||
      root.children[1].axis != ui::LayoutAxis::overlay) {
    return 1;
  }
  if (root.child_spacing != 20.0f || root.content_padding.left != 24.0f ||
      root.content_padding.right != 24.0f ||
      root.content_padding.bottom != 24.0f) {
    return 8;
  }

  const ui::View &toolbar = root.children[0];
  const ui::View &content_surface = root.children[1];

  if (toolbar.kind != ui::ViewKind::stack ||
      toolbar.axis != ui::LayoutAxis::horizontal ||
      toolbar.child_spacing != 24.0f ||
      toolbar.children.size() != 2 ||
      toolbar.children[0].kind != ui::ViewKind::button_group ||
      toolbar.children[1].kind != ui::ViewKind::text) {
    return 2;
  }
  if (!toolbar.leading_window_controls_placement.is_enabled ||
      toolbar.leading_window_controls_placement.spacing != 12.0f ||
      !toolbar.leading_window_controls_placement.aligns_vertical_center) {
    return 3;
  }

  const ui::View &navigation_group = toolbar.children[0];
  const ui::View &path_label = toolbar.children[1];
  if (navigation_group.children.size() != 2 ||
      navigation_group.control_shape != ui::ControlShape::capsule) {
    return 10;
  }
  if (navigation_group.children[0].button_role != ui::ButtonRole::back ||
      navigation_group.children[1].button_role != ui::ButtonRole::forward) {
    return 4;
  }
  if (navigation_group.children[0].children[0].symbol !=
          ui::Symbol::chevron_left ||
      navigation_group.children[1].children[0].symbol !=
          ui::Symbol::chevron_right) {
    return 5;
  }
  if (navigation_group.children[0].accessibility_label_text != "Back" ||
      navigation_group.children[1].accessibility_label_text != "Forward") {
    return 6;
  }
  if (path_label.text_content != "kakao" || path_label.font_size_value != 20.0f ||
      path_label.font_weight_value != 550.0f ||
      path_label.foreground_color.red != ui::primary_label().red ||
      path_label.foreground_color.alpha != ui::primary_label().alpha) {
    return 11;
  }
  if (!content_surface.expands_width || !content_surface.expands_height ||
      content_surface.children.size() != 2 ||
      content_surface.children[0].kind != ui::ViewKind::panel ||
      content_surface.children[1].kind != ui::ViewKind::grid) {
    return 9;
  }

  const ui::View &content_panel = content_surface.children[0];
  const ui::View &content_grid = content_surface.children[1];
  if (!content_panel.expands_width || !content_panel.expands_height ||
      content_panel.corner_radius_value != 18.0f ||
      content_panel.background_color.red != 0.985f ||
      content_panel.background_color.green != 0.988f ||
      content_panel.background_color.blue != 0.992f ||
      content_panel.background_color.alpha != 0.72f) {
    return 9;
  }
  if (!content_grid.expands_width || !content_grid.expands_height ||
      content_grid.grid_min_column_width != 112.0f ||
      content_grid.grid_row_height != 92.0f ||
      content_grid.grid_column_gap != 18.0f ||
      content_grid.grid_row_gap != 20.0f ||
      content_grid.children.size() != 2) {
    return 12;
  }
  if (!content_grid.children[0].centers_children ||
      content_grid.children[0].children[0].symbol != ui::Symbol::folder ||
      content_grid.children[1].children[0].symbol != ui::Symbol::description ||
      !content_grid.children[0].children[1].centers_text ||
      content_grid.children[0].children[1].preferred_size.width != 96.0f ||
      content_grid.children[0].children[1].text_line_limit != 1 ||
      content_grid.children[0].children[1].text_overflow !=
          ui::TextOverflow::ellipsis ||
      content_grid.children[0].children[1].text_truncation !=
          ui::TextTruncation::tail) {
    return 13;
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

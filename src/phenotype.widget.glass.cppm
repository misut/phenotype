module;
#include <algorithm>
export module phenotype.widget.glass;

import phenotype.material;
import phenotype.state;
import phenotype.types;

export namespace phenotype::widget {

inline Color glass_control_state_color(Color base,
                                       unsigned char minimum_alpha) noexcept {
    if (base.a < minimum_alpha)
        base.a = minimum_alpha;
    return base;
}

inline MaterialStyle plain_control_material(Color tint,
                                            Color border,
                                            Color foreground,
                                            MaterialSurfaceRole role) noexcept {
    auto material = material_style_for_kind(
        MaterialKind::Regular,
        detail::g_app.theme);
    material.role = role;
    material.allows_liquid_glass = false;
    material.opacity = 1.0f;
    material.blur_radius = 0.0f;
    material.tint = tint;
    material.border = border;
    material.foreground = foreground;
    material.saturation = 1.0f;
    material.luminance_floor = 0.0f;
    material.luminance_gain = 1.0f;
    material.edge_highlight = 0.0f;
    material.noise_opacity = 0.0f;
    material.shadow_alpha = 0.0f;
    material.shadow_radius = 0.0f;
    material.fallback = false;
    material.fallback_reason = "";
    material.contrast_intent = "plain-control";
    material.plan_id = "material.plain.control";
    material.verifier_profile = "regular-legibility-backdrop";
    return material;
}

inline MaterialStyle interaction_glass_material(
        MaterialKind kind,
        MaterialSurfaceRole role,
        Color tint,
        Color border,
        Color foreground) noexcept {
    auto material = material_style_for_kind(kind, detail::g_app.theme);
    material.role = role;
    material.fallback = kind != MaterialKind::None;
    material.tint = tint;
    material.border = border;
    material.foreground = foreground;
    material.container = MaterialContainerDescriptor{
        0u,
        0u,
        8.0f,
        kind != MaterialKind::None,
        true};
    material.contrast_intent = "interaction-glass-control";
    material.plan_id = "material.glass.control.interaction";
    material.verifier_profile = "interaction-control-glass";
    return material;
}

inline ButtonStyleOptions interaction_glass_button_style(
        ButtonStyleOptions style,
        MaterialSurfaceRole role = MaterialSurfaceRole::Control,
        MaterialKind hover_kind = MaterialKind::Clear,
        MaterialKind pressed_kind = MaterialKind::Regular) {
    auto const& t = detail::g_app.theme;
    auto const base_bg = style.has_background
        ? style.background
        : (style.variant == ButtonVariant::Primary ? t.accent : t.surface);
    auto const hover_bg = style.has_hover_background
        ? style.hover_background
        : (style.variant == ButtonVariant::Primary
            ? t.accent_strong
            : t.state_hover_bg);
    auto const pressed_bg = style.has_pressed_background
        ? style.pressed_background
        : (style.variant == ButtonVariant::Primary
            ? t.accent_strong
            : t.state_hover_bg);
    auto const border = style.has_border_color
        ? style.border_color
        : (style.variant == ButtonVariant::Primary ? t.accent : t.border);
    auto const text = style.has_text_color
        ? style.text_color
        : (style.variant == ButtonVariant::Primary
            ? t.state_active_fg
            : t.foreground);

    style.has_material = !style.disabled && base_bg.a > 0;
    if (style.has_material)
        style.material = plain_control_material(base_bg, border, text, role);

    style.has_hover_material =
        !style.disabled && hover_kind != MaterialKind::None && hover_bg.a > 0;
    if (style.has_hover_material) {
        style.hover_material =
            interaction_glass_material(hover_kind, role, hover_bg, border, text);
    }

    style.has_pressed_material =
        !style.disabled && pressed_kind != MaterialKind::None
        && pressed_bg.a > 0;
    if (style.has_pressed_material) {
        style.pressed_material = interaction_glass_material(
            pressed_kind,
            role,
            pressed_bg,
            border,
            text);
    }
    return style;
}

inline ButtonStyleOptions glass_control_button_style(
        GlassControlStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;
    auto const kind = options.selected && options.kind == MaterialKind::None
        ? MaterialKind::Thin
        : (options.prominent && options.kind == MaterialKind::Clear
            ? MaterialKind::Regular
            : options.kind);
    bool const custom_tint =
        options.has_tint && options.tint.a > 0 && kind != MaterialKind::None;
    bool const custom_border =
        options.has_border && options.border.a > 0 && kind != MaterialKind::None;
    auto material = material_style_for_kind(kind, t);
    material.role = options.role;
    material.fallback = kind != MaterialKind::None;
    material.container = MaterialContainerDescriptor{
        0u,
        0u,
        8.0f,
        !options.disabled && kind != MaterialKind::None,
        true};
    material.prominence = MaterialProminenceDescriptor{
        options.prominent && !options.disabled && kind != MaterialKind::None,
        options.prominence};
    if (options.prominent && kind != MaterialKind::None) {
        material.tint = material_with_alpha(t.accent, 178);
        material.border = material_with_alpha(t.accent_strong, 190);
        material.foreground = t.state_active_fg;
        material.edge_highlight = std::max(material.edge_highlight, 0.42f);
        material.shadow_alpha = std::max(material.shadow_alpha, 0.16f);
        material.contrast_intent = "prominent-action";
        material.plan_id = "material.glass.prominent.action";
        material.verifier_profile = "prominent-action-glass";
    } else {
        if (custom_tint) {
            material.tint = options.tint;
            material.edge_highlight =
                std::max(material.edge_highlight, 0.30f);
            material.contrast_intent = "tinted-glass-control";
            material.plan_id = "material.glass.control.tinted";
            material.verifier_profile = "tinted-control-glass";
        } else if (options.selected) {
            material.tint = material_with_alpha(t.surface, 198);
        }
        material.border = custom_border
            ? options.border
            : material_with_alpha(
                t.border,
                static_cast<unsigned char>(options.selected ? 190 : 120));
    }

    ButtonStyleOptions style;
    style.disabled = options.disabled;
    style.has_material = kind != MaterialKind::None;
    style.material = material;
    style.has_background = true;
    style.background = material.tint;
    style.has_hover_background = true;
    style.hover_background = options.prominent && kind != MaterialKind::None
        ? material_with_alpha(t.accent_strong, 204)
        : (custom_tint
            ? glass_control_state_color(
                options.tint,
                static_cast<unsigned char>(options.selected ? 222 : 174))
            : material_with_alpha(
                t.surface,
                static_cast<unsigned char>(options.selected ? 222 : 150)));
    style.has_pressed_background = true;
    style.pressed_background = options.prominent && kind != MaterialKind::None
        ? material_with_alpha(t.accent_strong, 228)
        : (custom_tint
            ? glass_control_state_color(
                options.tint,
                static_cast<unsigned char>(options.selected ? 238 : 204))
            : material_with_alpha(
                t.surface,
                static_cast<unsigned char>(options.selected ? 238 : 176)));
    style.has_border_color = true;
    style.border_color = material.border;
    style.has_text_color = true;
    style.text_color = options.prominent && kind != MaterialKind::None
        ? t.state_active_fg
        : (custom_tint || options.selected ? t.foreground : t.muted);
    style.border_width = options.prominent || options.selected ? 1.0f : 0.0f;
    style.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_md;
    style.shape = options.shape;
    style.font_size = options.font_size;
    style.max_width = options.width;
    style.fixed_height = options.height;
    style.min_hit_width = minimum_button_activation_size;
    style.min_hit_height = minimum_button_activation_size;
    style.text_align = options.text_align;
    return style;
}

inline ButtonStyleOptions interaction_glass_control_button_style(
        GlassControlStyleOptions options = {}) {
    auto const hover_kind = options.kind == MaterialKind::None
        ? MaterialKind::Clear
        : options.kind;
    auto const pressed_kind = options.prominent ? MaterialKind::Regular
        : (hover_kind == MaterialKind::None ? MaterialKind::Clear
                                           : MaterialKind::Regular);
    return interaction_glass_button_style(
        glass_control_button_style(options),
        options.role,
        hover_kind,
        pressed_kind);
}

inline ButtonStyleOptions glass_prominent_button_style(
        GlassControlStyleOptions options = {}) {
    options.kind = options.kind == MaterialKind::None
        ? MaterialKind::None
        : MaterialKind::Regular;
    options.role = MaterialSurfaceRole::Control;
    options.prominent = true;
    return glass_control_button_style(options);
}

inline ButtonStyleOptions glass_split_button_style(
        GlassSplitButtonStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;
    auto const kind = options.disabled ? MaterialKind::None : options.kind;
    auto material = material_style_for_kind(kind, t);
    material.role = options.role;
    material.fallback = kind != MaterialKind::None;
    material.container = MaterialContainerDescriptor{
        options.container_id,
        options.union_id,
        options.spacing,
        options.container_id != 0u && !options.disabled,
        options.container_id != 0u && !options.disabled};

    bool const grouped = options.segment != GlassSplitButtonSegment::Single;
    unsigned char const base_alpha = static_cast<unsigned char>(
        options.selected ? 186 : (grouped ? 126 : 112));
    unsigned char const hover_alpha = static_cast<unsigned char>(
        options.selected ? 214 : (grouped ? 156 : 148));
    unsigned char const pressed_alpha = static_cast<unsigned char>(
        options.selected ? 232 : (grouped ? 184 : 174));
    if (kind != MaterialKind::None) {
        material.tint = material_with_alpha(t.surface, base_alpha);
        material.border = material_with_alpha(
            t.border,
            static_cast<unsigned char>(options.selected ? 150 : 96));
        material.foreground = options.selected ? t.foreground : t.muted;
    }

    ButtonStyleOptions style;
    style.disabled = options.disabled;
    style.has_material = kind != MaterialKind::None;
    style.material = material;
    style.has_background = true;
    style.background = kind != MaterialKind::None
        ? material.tint
        : t.transparent;
    style.has_hover_background = true;
    style.hover_background = material_with_alpha(t.surface, hover_alpha);
    style.has_pressed_background = true;
    style.pressed_background = material_with_alpha(t.surface, pressed_alpha);
    style.has_border_color = true;
    style.border_color = kind != MaterialKind::None
        ? material.border
        : t.transparent;
    style.has_text_color = true;
    style.text_color = options.disabled
        ? t.state_disabled_fg
        : (options.selected ? t.foreground : t.muted);
    style.border_width = kind != MaterialKind::None
        ? (options.selected ? 1.0f : 0.0f)
        : 0.0f;
    style.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_md;
    style.shape = options.shape;
    style.font_size = options.font_size;
    style.max_width = options.width;
    style.fixed_height = options.height;
    style.min_hit_width = minimum_button_activation_size;
    style.min_hit_height = minimum_button_activation_size;
    style.text_align = options.text_align;
    return style;
}

inline ButtonStyleOptions glass_selection_button_style(
        GlassSelectionStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;
    auto const kind = options.selected
        ? options.selected_kind
        : options.unselected_kind;
    auto material = material_style_for_kind(kind, t);
    material.role = options.role;
    material.fallback = kind != MaterialKind::None;
    material.container = MaterialContainerDescriptor{
        0u,
        0u,
        12.0f,
        options.selected && !options.disabled,
        true};

    auto selected_bg = t.accent;
    auto selected_hover = t.accent_strong;
    auto selected_pressed = t.accent_strong;
    auto selected_text = t.state_active_fg;
    if (options.chrome == GlassSelectionChrome::SidebarPill) {
        selected_bg = material_with_alpha(t.code_bg, 150);
        selected_hover = material_with_alpha(t.code_bg, 176);
        selected_pressed = material_with_alpha(t.code_bg, 196);
        selected_text = t.accent;
    }
    if (kind != MaterialKind::None) {
        material.tint = selected_bg;
        material.border = material_with_alpha(
            t.border,
            options.chrome == GlassSelectionChrome::SidebarPill ? 0 : 90);
        material.foreground = selected_text;
        material.accent_foreground = t.accent;
        material.strong_accent_foreground = t.accent_strong;
    }

    ButtonStyleOptions style;
    style.disabled = options.disabled;
    style.has_material = kind != MaterialKind::None;
    style.material = material;
    style.has_background = true;
    style.background = options.selected ? selected_bg : t.transparent;
    style.has_hover_background = true;
    style.hover_background = options.selected
        ? selected_hover
        : material_with_alpha(t.surface, 110);
    style.has_pressed_background = true;
    style.pressed_background = options.selected
        ? selected_pressed
        : material_with_alpha(t.surface, 140);
    style.has_border_color = true;
    style.border_color = options.selected && kind != MaterialKind::None
        ? material.border
        : t.transparent;
    style.has_text_color = true;
    style.text_color = options.selected ? selected_text : t.foreground;
    style.border_width = 0.0f;
    style.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_md;
    style.shape = options.shape != MaterialSurfaceShape::Default
        ? options.shape
        : (options.chrome == GlassSelectionChrome::SidebarPill
            ? MaterialSurfaceShape::Capsule
            : MaterialSurfaceShape::Default);
    style.font_size = options.font_size;
    style.max_width = options.width;
    style.fixed_height = options.height;
    style.min_hit_width = minimum_button_activation_size;
    style.min_hit_height = minimum_button_activation_size;
    style.text_align = options.text_align;
    return style;
}

inline ButtonStyleOptions glass_outline_row_button_style(
        GlassOutlineRowStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;
    auto const kind = options.disabled ? MaterialKind::None
        : (options.selected ? options.selected_kind
           : (options.expanded ? options.expanded_kind
              : options.unselected_kind));
    auto material = material_style_for_kind(kind, t);
    material.role = options.role;
    material.fallback = kind != MaterialKind::None;
    material.container = MaterialContainerDescriptor{
        0u,
        0u,
        10.0f + static_cast<float>(options.depth) * 4.0f,
        (options.selected || options.expanded) && !options.disabled,
        true};

    bool const sidebar =
        options.chrome == GlassOutlineRowChrome::SidebarPill;
    bool const column = options.chrome == GlassOutlineRowChrome::ColumnRow;
    Color selected_bg = sidebar
        ? material_with_alpha(t.code_bg, 150)
        : (column ? material_with_alpha(t.surface, 172) : t.accent);
    Color selected_hover = sidebar
        ? material_with_alpha(t.code_bg, 176)
        : (column ? material_with_alpha(t.surface, 196) : t.accent_strong);
    Color selected_pressed = sidebar
        ? material_with_alpha(t.code_bg, 196)
        : (column ? material_with_alpha(t.surface, 216) : t.accent_strong);
    Color selected_text = sidebar || column ? t.accent : t.state_active_fg;
    auto const expanded_bg = material_with_alpha(t.surface, 122);
    auto const expanded_hover = material_with_alpha(t.surface, 154);
    auto const expanded_pressed = material_with_alpha(t.surface, 184);
    auto const idle_hover = material_with_alpha(t.surface, 96);
    auto const idle_pressed = material_with_alpha(t.surface, 126);

    if (kind != MaterialKind::None) {
        material.tint = options.selected ? selected_bg : expanded_bg;
        material.border = t.transparent;
        material.foreground = options.selected ? selected_text : t.foreground;
        material.accent_foreground = t.accent;
        material.strong_accent_foreground = t.accent_strong;
    }

    ButtonStyleOptions style;
    style.disabled = options.disabled;
    style.has_material = kind != MaterialKind::None;
    style.material = material;
    style.has_background = true;
    style.background = options.selected ? selected_bg
        : (options.expanded && kind != MaterialKind::None
           ? expanded_bg
           : t.transparent);
    style.has_hover_background = true;
    style.hover_background = options.selected ? selected_hover
        : (options.expanded ? expanded_hover : idle_hover);
    style.has_pressed_background = true;
    style.pressed_background = options.selected ? selected_pressed
        : (options.expanded ? expanded_pressed : idle_pressed);
    style.has_border_color = true;
    style.border_color = kind != MaterialKind::None
        ? material.border
        : t.transparent;
    style.has_text_color = true;
    style.text_color = options.disabled ? t.state_disabled_fg
        : (options.selected ? selected_text : t.foreground);
    style.border_width = 0.0f;
    style.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_md;
    style.shape = options.shape != MaterialSurfaceShape::Default
        ? options.shape
        : (sidebar ? MaterialSurfaceShape::Capsule
                   : MaterialSurfaceShape::Default);
    style.font_size = options.font_size;
    style.max_width = options.width;
    style.fixed_height = options.height;
    if (options.depth > 0u)
        style.padding_left = static_cast<float>(options.depth) * 14.0f;
    style.min_hit_width = minimum_button_activation_size;
    style.min_hit_height = minimum_button_activation_size;
    style.text_align = options.text_align;
    return style;
}

inline ButtonStyleOptions glass_menu_item_button_style(
        GlassMenuItemStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;
    auto const kind = options.disabled ? MaterialKind::None : options.kind;
    auto material = material_style_for_kind(kind, t);
    material.role = options.role;
    material.fallback = kind != MaterialKind::None;
    material.container = MaterialContainerDescriptor{
        0u,
        0u,
        8.0f,
        !options.disabled,
        true};

    ButtonStyleOptions style;
    style.disabled = options.disabled;
    style.has_material = kind != MaterialKind::None;
    style.material = material;
    style.has_background = true;
    style.background = kind != MaterialKind::None
        ? material.tint
        : t.transparent;
    style.has_hover_background = true;
    style.hover_background = material_with_alpha(t.surface, 150);
    style.has_pressed_background = true;
    style.pressed_background = material_with_alpha(t.surface, 188);
    style.has_border_color = true;
    style.border_color = kind != MaterialKind::None
        ? material.border
        : t.transparent;
    style.has_text_color = true;
    style.text_color = options.disabled ? t.state_disabled_fg : t.foreground;
    style.border_width = 0.0f;
    style.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_md;
    style.shape = options.shape;
    style.max_width = options.width;
    style.fixed_height = options.height;
    style.min_hit_width = minimum_button_activation_size;
    style.min_hit_height = minimum_button_activation_size;
    style.text_align = TextAlign::Center;
    return style;
}

inline ButtonStyleOptions glass_table_header_button_style(
        GlassTableHeaderStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;
    auto const kind = options.disabled ? MaterialKind::None : options.kind;
    auto material = material_style_for_kind(kind, t);
    material.role = options.role;
    material.fallback = kind != MaterialKind::None;
    material.tint = options.sorted
        ? material_with_alpha(t.surface, 170)
        : material_with_alpha(t.surface, 96);
    material.border = options.sorted
        ? material_with_alpha(t.border, 120)
        : t.transparent;
    material.foreground = options.sorted ? t.foreground : t.muted;
    material.container = MaterialContainerDescriptor{
        0u,
        0u,
        6.0f,
        !options.disabled,
        true};

    ButtonStyleOptions style;
    style.disabled = options.disabled;
    style.has_material = kind != MaterialKind::None;
    style.material = material;
    style.has_background = true;
    style.background = kind != MaterialKind::None
        ? material.tint
        : t.transparent;
    style.has_hover_background = true;
    style.hover_background = material_with_alpha(t.surface, 140);
    style.has_pressed_background = true;
    style.pressed_background = material_with_alpha(t.surface, 180);
    style.has_border_color = true;
    style.border_color = kind != MaterialKind::None
        ? material.border
        : t.transparent;
    style.has_text_color = true;
    style.text_color = options.disabled
        ? t.state_disabled_fg
        : (options.sorted ? t.foreground : t.muted);
    style.border_width = options.sorted && kind != MaterialKind::None
        ? 1.0f
        : 0.0f;
    style.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_sm;
    style.shape = options.shape;
    style.font_size = options.font_size;
    style.max_width = options.width;
    style.fixed_height = options.height;
    style.min_hit_width = minimum_button_activation_size;
    style.min_hit_height = minimum_button_activation_size;
    style.text_align = options.text_align;
    return style;
}

inline ButtonStyleOptions glass_disclosure_header_style(
        GlassDisclosureStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;
    auto const kind = options.disabled ? MaterialKind::None : options.kind;
    auto material = material_style_for_kind(kind, t);
    material.role = options.role;
    material.fallback = kind != MaterialKind::None;
    material.tint = options.expanded
        ? material_with_alpha(t.surface, 150)
        : material_with_alpha(t.surface, 102);
    material.border = material_with_alpha(
        t.border,
        static_cast<unsigned char>(options.expanded ? 128 : 92));
    material.foreground = t.foreground;
    material.container = MaterialContainerDescriptor{
        0u,
        0u,
        8.0f,
        !options.disabled,
        true};

    ButtonStyleOptions style;
    style.disabled = options.disabled;
    style.has_material = kind != MaterialKind::None;
    style.material = material;
    style.has_background = true;
    style.background = kind != MaterialKind::None
        ? material.tint
        : t.transparent;
    style.has_hover_background = true;
    style.hover_background = material_with_alpha(t.surface, 150);
    style.has_pressed_background = true;
    style.pressed_background = material_with_alpha(t.surface, 188);
    style.has_border_color = true;
    style.border_color = kind != MaterialKind::None
        ? material.border
        : t.transparent;
    style.has_text_color = true;
    style.text_color = options.disabled ? t.state_disabled_fg : t.foreground;
    style.border_width = kind != MaterialKind::None ? 1.0f : 0.0f;
    style.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_sm;
    style.shape = options.shape;
    style.font_size = options.font_size;
    style.max_width = options.width;
    style.fixed_height = options.height;
    style.min_hit_width = minimum_button_activation_size;
    style.min_hit_height = minimum_button_activation_size;
    style.text_align = options.text_align;
    return style;
}

inline TextFieldStyleOptions glass_text_field_style(
        GlassTextFieldStyleOptions options = {}) {
    auto const& t = detail::g_app.theme;
    auto material = material_style_for_kind(options.kind, t);
    material.role = options.role;
    material.fallback = options.kind != MaterialKind::None;
    material.container = MaterialContainerDescriptor{
        0u,
        0u,
        20.0f,
        !options.disabled,
        true};

    TextFieldStyleOptions style;
    style.error = options.error;
    style.disabled = options.disabled;
    style.has_material = options.kind != MaterialKind::None;
    style.material = material;
    style.has_background = true;
    style.background = material.tint;
    style.has_border_color = true;
    style.border_color = material.border;
    style.border_width = 0.0f;
    style.border_radius = options.border_radius >= 0.0f
        ? options.border_radius
        : t.radius_lg;
    style.shape = options.shape;
    style.font_size = options.font_size;
    style.max_width = options.width;
    style.fixed_height = options.height;
    style.semantic_label = options.semantic_label;
    return style;
}

} // namespace phenotype::widget

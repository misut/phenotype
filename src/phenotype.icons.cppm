module;
#include <cstdint>
#include <cstring>
#include <optional>
#include <string_view>
export module phenotype.icons;

import phenotype.svg;
import phenotype.icon_catalog;
import phenotype.types;

export namespace phenotype::icons {

enum class Symbol : unsigned int {
    Back,
    Forward,
    Search,
    Share,
    Tag,
    More,
    Grid,
    List,
    Columns,
    Gallery,
    Folder,
    Trash,
    Document,
    Image,
    Movie,
    Plus,
    XMark,
    ChevronDown,
    Home,
    Cloud,
    AirDrop,
    Recents,
    Shared,
    Sidebar,
    NewFolder,
    Applications,
    Desktop,
    Download,
    SortGroup,
    Duplicate,
    NewDocument,
    PdfDocument,
    TextDocument,
    Archive,
};

namespace catalog = phenotype::icon_catalog;

static_assert(catalog::all_symbol_count == 34);
static_assert(static_cast<unsigned int>(Symbol::Archive) + 1
              == catalog::all_symbol_count);

inline auto to_catalog_symbol(Symbol symbol) noexcept -> catalog::Symbol {
    return static_cast<catalog::Symbol>(static_cast<unsigned int>(symbol));
}

inline auto from_catalog_symbol(catalog::Symbol symbol) noexcept -> Symbol {
    return static_cast<Symbol>(static_cast<unsigned int>(symbol));
}

inline auto name(Symbol symbol) noexcept -> std::string_view {
    return catalog::name(to_catalog_symbol(symbol));
}

enum class SymbolRole {
    Navigation,
    Toolbar,
    Sidebar,
    FileType,
    Action,
};

enum class SymbolVariant {
    Outline,
    Fill,
};

enum class SymbolRenderingMode {
    Monochrome,
    Hierarchical,
};

enum class SymbolScale {
    Small,
    Medium,
    Large,
};

enum class SymbolWeight {
    Regular,
};

enum class SymbolStrokeCap {
    Round,
};

enum class SymbolStrokeJoin {
    Round,
};

enum class SymbolPresentationRole {
    Toolbar,
    Navigation,
    Sidebar,
    FileType,
    Action,
};

enum class SymbolTone {
    Primary,
    Secondary,
    Selected,
    Accent,
    Disabled,
    Destructive,
};

enum class SymbolInteractionPhase {
    Normal,
    Hovered,
    Pressed,
};

inline auto to_catalog_role(SymbolRole role) noexcept -> catalog::SymbolRole {
    return static_cast<catalog::SymbolRole>(static_cast<unsigned int>(role));
}

inline auto from_catalog_role(catalog::SymbolRole role) noexcept -> SymbolRole {
    return static_cast<SymbolRole>(static_cast<unsigned int>(role));
}

inline auto to_catalog_variant(SymbolVariant variant) noexcept
        -> catalog::SymbolVariant {
    return static_cast<catalog::SymbolVariant>(
        static_cast<unsigned int>(variant));
}

inline auto from_catalog_variant(catalog::SymbolVariant variant) noexcept
        -> SymbolVariant {
    return static_cast<SymbolVariant>(static_cast<unsigned int>(variant));
}

inline auto to_catalog_rendering(SymbolRenderingMode mode) noexcept
        -> catalog::SymbolRenderingMode {
    return static_cast<catalog::SymbolRenderingMode>(
        static_cast<unsigned int>(mode));
}

inline auto from_catalog_rendering(
        catalog::SymbolRenderingMode mode) noexcept -> SymbolRenderingMode {
    return static_cast<SymbolRenderingMode>(static_cast<unsigned int>(mode));
}

inline auto to_catalog_scale(SymbolScale scale) noexcept
        -> catalog::SymbolScale {
    return static_cast<catalog::SymbolScale>(static_cast<unsigned int>(scale));
}

inline auto from_catalog_scale(catalog::SymbolScale scale) noexcept
        -> SymbolScale {
    return static_cast<SymbolScale>(static_cast<unsigned int>(scale));
}

inline auto to_catalog_weight(SymbolWeight weight) noexcept
        -> catalog::SymbolWeight {
    return static_cast<catalog::SymbolWeight>(
        static_cast<unsigned int>(weight));
}

inline auto from_catalog_weight(catalog::SymbolWeight weight) noexcept
        -> SymbolWeight {
    return static_cast<SymbolWeight>(static_cast<unsigned int>(weight));
}

inline auto to_catalog_stroke_cap(SymbolStrokeCap cap) noexcept
        -> catalog::SymbolStrokeCap {
    return static_cast<catalog::SymbolStrokeCap>(
        static_cast<unsigned int>(cap));
}

inline auto from_catalog_stroke_cap(
        catalog::SymbolStrokeCap cap) noexcept -> SymbolStrokeCap {
    return static_cast<SymbolStrokeCap>(static_cast<unsigned int>(cap));
}

inline auto to_catalog_stroke_join(SymbolStrokeJoin join) noexcept
        -> catalog::SymbolStrokeJoin {
    return static_cast<catalog::SymbolStrokeJoin>(
        static_cast<unsigned int>(join));
}

inline auto from_catalog_stroke_join(
        catalog::SymbolStrokeJoin join) noexcept -> SymbolStrokeJoin {
    return static_cast<SymbolStrokeJoin>(static_cast<unsigned int>(join));
}

inline auto to_catalog_presentation_role(SymbolPresentationRole role) noexcept
        -> catalog::SymbolPresentationRole {
    return static_cast<catalog::SymbolPresentationRole>(
        static_cast<unsigned int>(role));
}

inline auto from_catalog_presentation_role(
        catalog::SymbolPresentationRole role) noexcept
        -> SymbolPresentationRole {
    return static_cast<SymbolPresentationRole>(
        static_cast<unsigned int>(role));
}

inline auto to_catalog_tone(SymbolTone tone) noexcept -> catalog::SymbolTone {
    return static_cast<catalog::SymbolTone>(static_cast<unsigned int>(tone));
}

inline auto from_catalog_tone(catalog::SymbolTone tone) noexcept -> SymbolTone {
    return static_cast<SymbolTone>(static_cast<unsigned int>(tone));
}

inline auto to_catalog_phase(SymbolInteractionPhase phase) noexcept
        -> catalog::SymbolInteractionPhase {
    return static_cast<catalog::SymbolInteractionPhase>(
        static_cast<unsigned int>(phase));
}

inline auto from_catalog_phase(
        catalog::SymbolInteractionPhase phase) noexcept
        -> SymbolInteractionPhase {
    return static_cast<SymbolInteractionPhase>(
        static_cast<unsigned int>(phase));
}

inline auto symbol_role_name(SymbolRole role) noexcept -> std::string_view {
    return catalog::symbol_role_name(to_catalog_role(role));
}

inline auto symbol_variant_name(SymbolVariant variant) noexcept
    -> std::string_view {
    return catalog::symbol_variant_name(to_catalog_variant(variant));
}

inline auto symbol_rendering_mode_name(SymbolRenderingMode mode) noexcept
    -> std::string_view {
    return catalog::symbol_rendering_mode_name(to_catalog_rendering(mode));
}

inline auto symbol_scale_name(SymbolScale scale) noexcept -> std::string_view {
    return catalog::symbol_scale_name(to_catalog_scale(scale));
}

inline auto symbol_weight_name(SymbolWeight weight) noexcept
        -> std::string_view {
    return catalog::symbol_weight_name(to_catalog_weight(weight));
}

inline auto symbol_stroke_cap_name(SymbolStrokeCap cap) noexcept
        -> std::string_view {
    return catalog::symbol_stroke_cap_name(to_catalog_stroke_cap(cap));
}

inline auto symbol_stroke_join_name(SymbolStrokeJoin join) noexcept
        -> std::string_view {
    return catalog::symbol_stroke_join_name(to_catalog_stroke_join(join));
}

inline auto symbol_presentation_role_name(
        SymbolPresentationRole role) noexcept -> std::string_view {
    return catalog::symbol_presentation_role_name(
        to_catalog_presentation_role(role));
}

inline auto symbol_tone_name(SymbolTone tone) noexcept -> std::string_view {
    return catalog::symbol_tone_name(to_catalog_tone(tone));
}

inline auto symbol_interaction_phase_name(
        SymbolInteractionPhase phase) noexcept -> std::string_view {
    return catalog::symbol_interaction_phase_name(to_catalog_phase(phase));
}

struct SymbolDescriptor {
    Symbol symbol = Symbol::Document;
    std::string_view name;
    SymbolRole role = SymbolRole::Toolbar;
    SymbolVariant variant = SymbolVariant::Outline;
    SymbolRenderingMode preferred_rendering = SymbolRenderingMode::Monochrome;
    SymbolScale default_scale = SymbolScale::Medium;
    SymbolWeight default_weight = SymbolWeight::Regular;
    std::string_view style;
    std::string_view semantic_reference_name;
    std::string_view reference_family;
    std::string_view reference_policy;
    float grid_size = 24.0f;
    float default_stroke_width = 1.8f;
    SymbolStrokeCap stroke_cap = SymbolStrokeCap::Round;
    SymbolStrokeJoin stroke_join = SymbolStrokeJoin::Round;
    float secondary_opacity = 1.0f;
    unsigned int layer_count = 1;
    bool uses_current_color = true;
    bool round_stroke = true;
    bool filled = false;
    bool text_weight_aligned = true;
    bool supports_monochrome = true;
    bool supports_hierarchical_opacity = false;
    bool supports_palette = false;
    bool supports_multicolor = false;
    bool phenotype_owned = true;
    bool uses_sf_symbols_asset = false;
};

struct SymbolRenderingCapabilities {
    bool monochrome = true;
    bool hierarchical = false;
    bool palette = false;
    bool multicolor = false;
    std::string_view policy;
};

struct SymbolPresentation {
    Symbol symbol = Symbol::Document;
    SymbolPresentationRole role = SymbolPresentationRole::Toolbar;
    SymbolTone tone = SymbolTone::Secondary;
    SymbolScale scale = SymbolScale::Medium;
    SymbolRenderingMode rendering = SymbolRenderingMode::Monochrome;
    Color color = {96, 96, 100, 255};
    float point_size = 24.0f;
    float hit_target_size = 36.0f;
    float optical_y_offset = 0.0f;
};

struct SymbolInteractionState {
    bool selected = false;
    bool enabled = true;
};

struct SymbolControlChrome {
    SymbolPresentationRole role = SymbolPresentationRole::Toolbar;
    SymbolTone symbol_tone = SymbolTone::Secondary;
    Color symbol_color = {96, 96, 100, 255};
    Color background_color = {0, 0, 0, 0};
    Color hover_background_color = {0, 0, 0, 0};
    float corner_radius = 0.0f;
    float hit_target_size = 36.0f;
    bool borderless = true;
    bool grouped_control = true;
    std::string_view policy;
};

struct SymbolStateRecipe {
    SymbolPresentationRole role = SymbolPresentationRole::Toolbar;
    SymbolInteractionPhase phase = SymbolInteractionPhase::Normal;
    bool selected = false;
    bool enabled = true;
    SymbolTone symbol_tone = SymbolTone::Secondary;
    Color symbol_color = {96, 96, 100, 255};
    Color background_color = {0, 0, 0, 0};
    float symbol_opacity = 1.0f;
    float scale = 1.0f;
    float corner_radius = 0.0f;
    float hit_target_size = 36.0f;
    bool borderless = true;
    bool grouped_control = true;
    std::string_view policy;
};

struct SymbolMetrics {
    SymbolPresentationRole role = SymbolPresentationRole::Toolbar;
    SymbolScale scale = SymbolScale::Medium;
    float grid_size = 24.0f;
    float point_size = 24.0f;
    float hit_target_size = 36.0f;
    float content_inset = 6.0f;
    float stroke_width = 1.8f;
    float optical_y_offset = 0.0f;
};

inline constexpr unsigned int all_symbol_count = catalog::all_symbol_count;
inline constexpr unsigned int sidebar_symbol_count =
    catalog::sidebar_symbol_count;
inline constexpr unsigned int toolbar_symbol_count =
    catalog::toolbar_symbol_count;
inline constexpr unsigned int outline_symbol_count =
    catalog::outline_symbol_count;
inline constexpr unsigned int filled_symbol_count =
    catalog::filled_symbol_count;
inline constexpr unsigned int hierarchical_symbol_count =
    catalog::hierarchical_symbol_count;
inline constexpr unsigned int monochrome_symbol_count =
    catalog::monochrome_symbol_count;
inline constexpr unsigned int regular_weight_symbol_count =
    catalog::regular_weight_symbol_count;
inline constexpr unsigned int palette_symbol_count =
    catalog::palette_symbol_count;
inline constexpr unsigned int multicolor_symbol_count =
    catalog::multicolor_symbol_count;
inline constexpr unsigned int reference_symbol_count =
    catalog::reference_symbol_count;
inline constexpr unsigned int svg_path_arc_symbol_count =
    catalog::svg_path_arc_symbol_count;
inline constexpr unsigned int round_stroke_symbol_count =
    catalog::round_stroke_symbol_count;
inline constexpr unsigned int symbol_interaction_phase_count =
    catalog::symbol_interaction_phase_count;

inline auto symbol_at(unsigned int index) noexcept -> Symbol {
    return from_catalog_symbol(catalog::symbol_at(index));
}

inline auto sidebar_symbol_at(unsigned int index) noexcept -> Symbol {
    return from_catalog_symbol(catalog::sidebar_symbol_at(index));
}

inline auto toolbar_symbol_at(unsigned int index) noexcept -> Symbol {
    return from_catalog_symbol(catalog::toolbar_symbol_at(index));
}

inline auto symbol_from_name(std::string_view symbol_name) noexcept
        -> std::optional<Symbol> {
    auto const found = catalog::symbol_from_name(symbol_name);
    if (!found.has_value())
        return std::nullopt;
    return from_catalog_symbol(*found);
}

inline auto style_name() noexcept -> std::string_view {
    return catalog::style_name();
}

inline auto style_reference() noexcept -> std::string_view {
    return catalog::style_reference();
}

inline auto asset_policy() noexcept -> std::string_view {
    return catalog::asset_policy();
}

inline auto reference_family() noexcept -> std::string_view {
    return catalog::reference_family();
}

inline auto reference_policy() noexcept -> std::string_view {
    return catalog::reference_policy();
}

inline auto presentation_policy() noexcept -> std::string_view {
    return catalog::presentation_policy();
}

inline auto source_format() noexcept -> std::string_view {
    return catalog::source_format();
}

inline auto svg_subset_policy() noexcept -> std::string_view {
    return catalog::svg_subset_policy();
}

inline auto svg_supported_elements() noexcept -> std::string_view {
    return catalog::svg_supported_elements();
}

inline auto svg_supported_path_commands() noexcept -> std::string_view {
    return catalog::svg_supported_path_commands();
}

inline auto svg_supported_style_attributes() noexcept -> std::string_view {
    return catalog::svg_supported_style_attributes();
}

inline auto svg_arc_policy() noexcept -> std::string_view {
    return catalog::svg_arc_policy();
}

inline auto stroke_geometry_policy() noexcept -> std::string_view {
    return catalog::stroke_geometry_policy();
}

inline auto stroke_cap_policy() noexcept -> std::string_view {
    return catalog::stroke_cap_policy();
}

inline auto stroke_join_policy() noexcept -> std::string_view {
    return catalog::stroke_join_policy();
}

inline auto alignment_policy() noexcept -> std::string_view {
    return catalog::alignment_policy();
}

inline auto variant_policy() noexcept -> std::string_view {
    return catalog::variant_policy();
}

inline auto tone_policy() noexcept -> std::string_view {
    return catalog::tone_policy();
}

inline auto default_scale_policy() noexcept -> std::string_view {
    return catalog::default_scale_policy();
}

inline auto default_weight_policy() noexcept -> std::string_view {
    return catalog::default_weight_policy();
}

inline auto rendering_capability_policy() noexcept -> std::string_view {
    return catalog::rendering_capability_policy();
}

inline auto metrics_policy() noexcept -> std::string_view {
    return catalog::metrics_policy();
}

inline auto hit_target_policy() noexcept -> std::string_view {
    return catalog::hit_target_policy();
}

inline auto interface_metaphor_policy() noexcept -> std::string_view {
    return catalog::interface_metaphor_policy();
}

inline auto visual_consistency_policy() noexcept -> std::string_view {
    return catalog::visual_consistency_policy();
}

inline auto toolbar_symbol_chrome_policy() noexcept -> std::string_view {
    return catalog::toolbar_symbol_chrome_policy();
}

inline auto sidebar_symbol_color_policy() noexcept -> std::string_view {
    return catalog::sidebar_symbol_color_policy();
}

inline auto symbol_control_chrome_policy() noexcept -> std::string_view {
    return catalog::symbol_control_chrome_policy();
}

inline auto symbol_interaction_phase_policy() noexcept -> std::string_view {
    return catalog::symbol_interaction_phase_policy();
}

inline auto semantic_reference_name(Symbol symbol) noexcept -> std::string_view {
    return catalog::semantic_reference_name(to_catalog_symbol(symbol));
}

inline auto symbol_from_semantic_reference_name(
        std::string_view reference_name) noexcept -> std::optional<Symbol> {
    auto const found =
        catalog::symbol_from_semantic_reference_name(reference_name);
    if (!found.has_value())
        return std::nullopt;
    return from_catalog_symbol(*found);
}

inline bool supports_hierarchical_opacity(Symbol symbol) noexcept {
    return catalog::supports_hierarchical_opacity(to_catalog_symbol(symbol));
}

inline auto symbol_layer_count(Symbol symbol) noexcept -> unsigned int {
    return catalog::symbol_layer_count(to_catalog_symbol(symbol));
}

inline bool uses_svg_path_arcs(Symbol symbol) noexcept {
    return catalog::uses_svg_path_arcs(to_catalog_symbol(symbol));
}

inline auto rendering_capabilities(Symbol symbol) noexcept
        -> SymbolRenderingCapabilities {
    auto const base = catalog::rendering_capabilities(to_catalog_symbol(symbol));
    return SymbolRenderingCapabilities{
        base.monochrome,
        base.hierarchical,
        base.palette,
        base.multicolor,
        base.policy,
    };
}

inline auto descriptor(Symbol symbol) noexcept -> SymbolDescriptor {
    auto const base = catalog::descriptor(to_catalog_symbol(symbol));
    return SymbolDescriptor{
        symbol,
        base.name,
        from_catalog_role(base.role),
        from_catalog_variant(base.variant),
        from_catalog_rendering(base.preferred_rendering),
        from_catalog_scale(base.default_scale),
        from_catalog_weight(base.default_weight),
        base.style,
        base.semantic_reference_name,
        base.reference_family,
        base.reference_policy,
        base.grid_size,
        base.default_stroke_width,
        from_catalog_stroke_cap(base.stroke_cap),
        from_catalog_stroke_join(base.stroke_join),
        base.secondary_opacity,
        base.layer_count,
        base.uses_current_color,
        base.round_stroke,
        base.filled,
        base.text_weight_aligned,
        base.supports_monochrome,
        base.supports_hierarchical_opacity,
        base.supports_palette,
        base.supports_multicolor,
        base.phenotype_owned,
        base.uses_sf_symbols_asset,
    };
}

inline auto default_presentation_role(Symbol symbol) noexcept
        -> SymbolPresentationRole {
    return from_catalog_presentation_role(
        catalog::default_presentation_role(to_catalog_symbol(symbol)));
}

inline auto default_scale(SymbolPresentationRole role) noexcept
        -> SymbolScale {
    return from_catalog_scale(
        catalog::default_scale(to_catalog_presentation_role(role)));
}

inline float point_size(SymbolScale scale) noexcept {
    return catalog::point_size(to_catalog_scale(scale));
}

inline float hit_target_size(SymbolPresentationRole role) noexcept {
    return catalog::hit_target_size(to_catalog_presentation_role(role));
}

inline float optical_y_offset(SymbolPresentationRole role) noexcept {
    return catalog::optical_y_offset(to_catalog_presentation_role(role));
}

inline auto metrics(SymbolPresentationRole role) noexcept -> SymbolMetrics {
    auto const base = catalog::metrics(to_catalog_presentation_role(role));
    return SymbolMetrics{
        from_catalog_presentation_role(base.role),
        from_catalog_scale(base.scale),
        base.grid_size,
        base.point_size,
        base.hit_target_size,
        base.content_inset,
        base.stroke_width,
        base.optical_y_offset,
    };
}

inline auto symbol_metrics(Symbol symbol) noexcept -> SymbolMetrics {
    auto const base = catalog::symbol_metrics(to_catalog_symbol(symbol));
    return SymbolMetrics{
        from_catalog_presentation_role(base.role),
        from_catalog_scale(base.scale),
        base.grid_size,
        base.point_size,
        base.hit_target_size,
        base.content_inset,
        base.stroke_width,
        base.optical_y_offset,
    };
}

inline auto macos_light_tone_color(SymbolTone tone) noexcept -> Color {
    auto const color = catalog::macos_light_tone_color(to_catalog_tone(tone));
    return {color.r, color.g, color.b, color.a};
}

inline auto to_color(catalog::SymbolColor color) noexcept -> Color {
    return {color.r, color.g, color.b, color.a};
}

inline auto interaction_tone_policy() noexcept -> std::string_view {
    return catalog::interaction_tone_policy();
}

inline auto macos_interaction_tone(SymbolPresentationRole role,
                                   SymbolInteractionState state) noexcept
        -> SymbolTone {
    return from_catalog_tone(catalog::macos_interaction_tone(
        to_catalog_presentation_role(role),
        catalog::SymbolInteractionState{
            state.selected,
            state.enabled,
        }));
}

inline auto macos_interaction_tone(Symbol symbol,
                                   SymbolInteractionState state) noexcept
        -> SymbolTone {
    return from_catalog_tone(catalog::macos_interaction_tone(
        to_catalog_symbol(symbol),
        catalog::SymbolInteractionState{
            state.selected,
            state.enabled,
        }));
}

inline auto macos_control_chrome(SymbolPresentationRole role,
                                 SymbolInteractionState state) noexcept
        -> SymbolControlChrome {
    auto const base = catalog::macos_control_chrome(
        to_catalog_presentation_role(role),
        catalog::SymbolInteractionState{
            state.selected,
            state.enabled,
        });
    return SymbolControlChrome{
        from_catalog_presentation_role(base.role),
        from_catalog_tone(base.symbol_tone),
        to_color(base.symbol_color),
        to_color(base.background_color),
        to_color(base.hover_background_color),
        base.corner_radius,
        base.hit_target_size,
        base.borderless,
        base.grouped_control,
        base.policy,
    };
}

inline auto macos_state_recipe(SymbolPresentationRole role,
                               SymbolInteractionState state,
                               SymbolInteractionPhase phase) noexcept
        -> SymbolStateRecipe {
    auto const base = catalog::macos_state_recipe(
        to_catalog_presentation_role(role),
        catalog::SymbolInteractionState{
            state.selected,
            state.enabled,
        },
        to_catalog_phase(phase));
    return SymbolStateRecipe{
        from_catalog_presentation_role(base.role),
        from_catalog_phase(base.phase),
        base.selected,
        base.enabled,
        from_catalog_tone(base.symbol_tone),
        to_color(base.symbol_color),
        to_color(base.background_color),
        base.symbol_opacity,
        base.scale,
        base.corner_radius,
        base.hit_target_size,
        base.borderless,
        base.grouped_control,
        base.policy,
    };
}

inline auto macos_file_type_color(Symbol symbol) noexcept -> Color {
    auto const color = catalog::macos_file_type_color(to_catalog_symbol(symbol));
    return {color.r, color.g, color.b, color.a};
}

inline auto presentation(Symbol symbol,
                         SymbolPresentationRole role,
                         SymbolTone tone,
                         SymbolScale scale) noexcept
        -> SymbolPresentation {
    auto const desc = descriptor(symbol);
    return SymbolPresentation{
        symbol,
        role,
        tone,
        scale,
        desc.preferred_rendering,
        macos_light_tone_color(tone),
        point_size(scale),
        hit_target_size(role),
        optical_y_offset(role),
    };
}

inline auto presentation(Symbol symbol,
                         SymbolPresentationRole role,
                         SymbolTone tone = SymbolTone::Secondary) noexcept
        -> SymbolPresentation {
    return presentation(symbol, role, tone, default_scale(role));
}

inline auto presentation(Symbol symbol,
                         SymbolTone tone = SymbolTone::Secondary) noexcept
        -> SymbolPresentation {
    auto const role = default_presentation_role(symbol);
    return presentation(symbol, role, tone, default_scale(role));
}

inline auto symbol_color_with_opacity(Color color, float opacity) noexcept
        -> Color {
    if (opacity < 0.0f)
        opacity = 0.0f;
    if (opacity > 1.0f)
        opacity = 1.0f;
    auto const alpha =
        static_cast<unsigned int>(static_cast<float>(color.a) * opacity + 0.5f);
    color.a = static_cast<unsigned char>(alpha > 255u ? 255u : alpha);
    return color;
}

inline auto macos_presentation(Symbol symbol,
                               SymbolPresentationRole role,
                               SymbolInteractionState state,
                               SymbolInteractionPhase phase) noexcept
        -> SymbolPresentation {
    auto const recipe = macos_state_recipe(role, state, phase);
    auto out = presentation(symbol, role, recipe.symbol_tone);
    out.color = symbol_color_with_opacity(recipe.symbol_color,
                                          recipe.symbol_opacity);
    out.point_size *= recipe.scale;
    return out;
}

inline auto macos_presentation(Symbol symbol,
                               SymbolInteractionState state,
                               SymbolInteractionPhase phase) noexcept
        -> SymbolPresentation {
    return macos_presentation(
        symbol,
        default_presentation_role(symbol),
        state,
        phase);
}

inline auto source(Symbol symbol) noexcept -> std::string_view {
    return catalog::svg_source(to_catalog_symbol(symbol));
}

auto document(Symbol symbol) -> svg::Document {
    return svg::parse(source(symbol));
}

void paint_symbol(Painter& painter,
                  Symbol symbol,
                  float x,
                  float y,
                  float size,
                  Color color) {
    auto doc = document(symbol);
    svg::paint(painter, doc, x, y, size, size, svg::RenderOptions{color, true});
}

void paint_symbol(Painter& painter,
                  SymbolPresentation const& style,
                  float x,
                  float y) {
    paint_symbol(
        painter,
        style.symbol,
        x,
        y + style.optical_y_offset,
        style.point_size,
        style.color);
}

void paint_symbol_centered(Painter& painter,
                           Symbol symbol,
                           float box_x,
                           float box_y,
                           float box_width,
                           float box_height,
                           float size,
                           Color color) {
    auto const x = box_x + (box_width - size) * 0.5f;
    auto const y = box_y + (box_height - size) * 0.5f;
    paint_symbol(painter, symbol, x, y, size, color);
}

void paint_symbol_centered(Painter& painter,
                           SymbolPresentation const& style,
                           float box_x,
                           float box_y,
                           float box_width,
                           float box_height) {
    auto const x = box_x + (box_width - style.point_size) * 0.5f;
    auto const y = box_y + (box_height - style.point_size) * 0.5f;
    paint_symbol(painter, style, x, y);
}

inline auto paint_token(Symbol symbol, float size, Color color) noexcept
    -> std::uint64_t {
    std::uint64_t h = 1469598103934665603ull;
    auto mix = [&](std::uint64_t v) {
        h ^= v;
        h *= 1099511628211ull;
    };
    mix(static_cast<std::uint64_t>(symbol));
    unsigned int bits = 0;
    std::memcpy(&bits, &size, 4);
    mix(bits);
    mix(color.r);
    mix(color.g);
    mix(color.b);
    mix(color.a);
    return h == 0 ? 1 : h;
}

inline auto paint_token(SymbolPresentation const& style) noexcept
    -> std::uint64_t {
    auto h = paint_token(style.symbol, style.point_size, style.color);
    h ^= static_cast<std::uint64_t>(style.role) << 8;
    h ^= static_cast<std::uint64_t>(style.tone) << 16;
    h ^= static_cast<std::uint64_t>(style.scale) << 24;
    h ^= static_cast<std::uint64_t>(style.rendering) << 32;
    return h == 0 ? 1 : h;
}

} // namespace phenotype::icons

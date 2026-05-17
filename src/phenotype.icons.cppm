module;
#include <cstdint>
#include <cstring>
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
};

namespace catalog = phenotype::icon_catalog;

static_assert(catalog::all_symbol_count == 31);
static_assert(static_cast<unsigned int>(Symbol::NewDocument) + 1
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

inline auto symbol_presentation_role_name(
        SymbolPresentationRole role) noexcept -> std::string_view {
    return catalog::symbol_presentation_role_name(
        to_catalog_presentation_role(role));
}

inline auto symbol_tone_name(SymbolTone tone) noexcept -> std::string_view {
    return catalog::symbol_tone_name(to_catalog_tone(tone));
}

struct SymbolDescriptor {
    Symbol symbol = Symbol::Document;
    std::string_view name;
    SymbolRole role = SymbolRole::Toolbar;
    SymbolVariant variant = SymbolVariant::Outline;
    SymbolRenderingMode preferred_rendering = SymbolRenderingMode::Monochrome;
    SymbolScale default_scale = SymbolScale::Medium;
    std::string_view style;
    std::string_view semantic_reference_name;
    std::string_view reference_family;
    std::string_view reference_policy;
    float grid_size = 24.0f;
    float default_stroke_width = 1.8f;
    float secondary_opacity = 1.0f;
    unsigned int layer_count = 1;
    bool uses_current_color = true;
    bool round_stroke = true;
    bool filled = false;
    bool text_weight_aligned = true;
    bool supports_hierarchical_opacity = false;
    bool phenotype_owned = true;
    bool uses_sf_symbols_asset = false;
};

struct SymbolPresentation {
    Symbol symbol = Symbol::Document;
    SymbolPresentationRole role = SymbolPresentationRole::Toolbar;
    SymbolTone tone = SymbolTone::Secondary;
    SymbolScale scale = SymbolScale::Medium;
    SymbolRenderingMode rendering = SymbolRenderingMode::Monochrome;
    Color color = {96, 96, 100, 255};
    float point_size = 24.0f;
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
inline constexpr unsigned int reference_symbol_count =
    catalog::reference_symbol_count;

inline auto symbol_at(unsigned int index) noexcept -> Symbol {
    return from_catalog_symbol(catalog::symbol_at(index));
}

inline auto sidebar_symbol_at(unsigned int index) noexcept -> Symbol {
    return from_catalog_symbol(catalog::sidebar_symbol_at(index));
}

inline auto toolbar_symbol_at(unsigned int index) noexcept -> Symbol {
    return from_catalog_symbol(catalog::toolbar_symbol_at(index));
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

inline auto semantic_reference_name(Symbol symbol) noexcept -> std::string_view {
    return catalog::semantic_reference_name(to_catalog_symbol(symbol));
}

inline bool supports_hierarchical_opacity(Symbol symbol) noexcept {
    return catalog::supports_hierarchical_opacity(to_catalog_symbol(symbol));
}

inline auto symbol_layer_count(Symbol symbol) noexcept -> unsigned int {
    return catalog::symbol_layer_count(to_catalog_symbol(symbol));
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
        base.style,
        base.semantic_reference_name,
        base.reference_family,
        base.reference_policy,
        base.grid_size,
        base.default_stroke_width,
        base.secondary_opacity,
        base.layer_count,
        base.uses_current_color,
        base.round_stroke,
        base.filled,
        base.text_weight_aligned,
        base.supports_hierarchical_opacity,
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

inline float optical_y_offset(SymbolPresentationRole role) noexcept {
    return catalog::optical_y_offset(to_catalog_presentation_role(role));
}

inline auto macos_light_tone_color(SymbolTone tone) noexcept -> Color {
    auto const color = catalog::macos_light_tone_color(to_catalog_tone(tone));
    return {color.r, color.g, color.b, color.a};
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

inline auto source(Symbol symbol) noexcept -> std::string_view {
    switch (symbol) {
    case Symbol::Back:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M15.5 5 L8.5 12 L15.5 19"/></svg>)SVG";
    case Symbol::Forward:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M8.5 5 L15.5 12 L8.5 19"/></svg>)SVG";
    case Symbol::Search:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round"><circle cx="10.5" cy="10.5" r="5.8"/><path d="M15 15 L20 20"/></svg>)SVG";
    case Symbol::Share:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4 L12 15"/><path d="M8 8 L12 4 L16 8"/><path d="M6 12 L6 20 L18 20 L18 12" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Tag:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4 12 L12 4 L20 12 L12 20 Z"/><circle cx="12" cy="9" r="1.2" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::More:
        return R"SVG(<svg viewBox="0 0 24 24" fill="currentColor" stroke="none"><circle cx="6" cy="12" r="1.5"/><circle cx="12" cy="12" r="1.5"/><circle cx="18" cy="12" r="1.5"/></svg>)SVG";
    case Symbol::Grid:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="4" width="6" height="6" rx="1.4"/><rect x="14" y="4" width="6" height="6" rx="1.4"/><rect x="4" y="14" width="6" height="6" rx="1.4"/><rect x="14" y="14" width="6" height="6" rx="1.4"/></svg>)SVG";
    case Symbol::List:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M8 6 L20 6"/><path d="M8 12 L20 12"/><path d="M8 18 L20 18"/><circle cx="4.5" cy="6" r="1" stroke-opacity="0.66"/><circle cx="4.5" cy="12" r="1" stroke-opacity="0.66"/><circle cx="4.5" cy="18" r="1" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Columns:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="14" rx="2.4"/><path d="M9.3 5 L9.3 19" stroke-opacity="0.66"/><path d="M14.7 5 L14.7 19" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Gallery:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="11" rx="2.4"/><path d="M7 20 L17 20" stroke-opacity="0.66"/><path d="M8 16 L16 16" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Folder:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M4 8.5 L9.1 8.5 L11.1 10.4 L20 10.4 L20 19 L4 19 Z"/></svg>)SVG";
    case Symbol::Trash:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 8 L17 8"/><path d="M10 5 L14 5 L15 8"/><path d="M8 8 L9 20 L15 20 L16 8"/><path d="M11 11 L11 17" stroke-opacity="0.66"/><path d="M13 11 L13 17" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Document:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 4 L14 4 L18 8 L18 20 L7 20 Z"/><path d="M14 4 L14 8 L18 8" stroke-opacity="0.66"/><path d="M9.5 13 L15.5 13" stroke-opacity="0.66"/><path d="M9.5 16 L15.5 16" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Image:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="14" rx="2.4"/><circle cx="9" cy="10" r="1.5" stroke-opacity="0.66"/><polyline points="6.5 17 11 12.5 14 15.5 16 13.5 19 17" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Movie:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="6" width="16" height="12" rx="2.4"/><path d="M8 6 L8 18" stroke-opacity="0.66"/><path d="M16 6 L16 18" stroke-opacity="0.66"/><path d="M4 10 L20 10" stroke-opacity="0.66"/><path d="M4 14 L20 14" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Plus:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 5 L12 19"/><path d="M5 12 L19 12"/></svg>)SVG";
    case Symbol::XMark:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6.5 6.5 L17.5 17.5"/><path d="M17.5 6.5 L6.5 17.5"/></svg>)SVG";
    case Symbol::ChevronDown:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6.5 9 L12 14.5 L17.5 9"/></svg>)SVG";
    case Symbol::Home:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M4 11.5 L12 5 L20 11.5"/><path d="M6.5 10 L6.5 20 L17.5 20 L17.5 10"/><path d="M10 20 L10 14 L14 14 L14 20" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Cloud:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 18 L17 18 C20 18 21.5 16 21.5 13.7 C21.5 11.5 19.8 9.8 17.6 9.7 C16.8 7.3 14.7 6 12.2 6 C9.4 6 7.3 7.8 6.7 10.3 C4.4 10.7 2.5 12.2 2.5 14.4 C2.5 16.4 4.1 18 7 18 Z"/></svg>)SVG";
    case Symbol::AirDrop:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="1.25"/><circle cx="12" cy="12" r="4.3" stroke-opacity="0.66"/><circle cx="12" cy="12" r="7.3" stroke-opacity="0.42"/><path d="M12 13.4 L12 19.2" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Recents:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="8"/><path d="M12 7 L12 12 L8.5 12" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Shared:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M3.8 8.4 L8.7 8.4 L10.5 10.1 L19.4 10.1 L19.4 18.2 L3.8 18.2 Z"/><circle cx="17.2" cy="7.6" r="2.2" stroke-opacity="0.66"/><path d="M13.8 12.5 C14.5 11.2 15.7 10.6 17.2 10.6 C18.7 10.6 20 11.2 20.6 12.5" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Sidebar:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="14" rx="2.4"/><path d="M9 5 L9 19" stroke-opacity="0.66"/><path d="M6.2 8 L7.1 8" stroke-opacity="0.66"/><path d="M6.2 11 L7.1 11" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::NewFolder:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M4 8.5 L9.1 8.5 L11.1 10.4 L20 10.4 L20 19 L4 19 Z"/><path d="M14 13 L14 17" stroke-opacity="0.66"/><path d="M12 15 L16 15" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Applications:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round"><path d="M6 20 L12 4 L18 20"/><path d="M8.2 14.5 L15.8 14.5" stroke-opacity="0.66"/><path d="M4.5 20 L19.5 20" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Desktop:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="12" rx="2.4"/><path d="M9 20 L15 20" stroke-opacity="0.66"/><path d="M7 21.5 L17 21.5" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Download:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="8"/><path d="M12 7 L12 15"/><path d="M8.5 12 L12 15.5 L15.5 12"/></svg>)SVG";
    case Symbol::SortGroup:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="4.2" height="3" rx="0.8"/><rect x="10" y="5" width="4.2" height="3" rx="0.8"/><rect x="16" y="5" width="4.2" height="3" rx="0.8"/><rect x="4" y="11" width="4.2" height="3" rx="0.8"/><rect x="10" y="11" width="4.2" height="3" rx="0.8"/><rect x="16" y="11" width="4.2" height="3" rx="0.8"/><rect x="4" y="17" width="4.2" height="3" rx="0.8"/><rect x="10" y="17" width="4.2" height="3" rx="0.8"/><path d="M17 16 L19.2 18.2 L21.4 16" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Duplicate:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="6" y="8" width="10" height="12" rx="2" stroke-opacity="0.66"/><rect x="10" y="4" width="10" height="12" rx="2"/></svg>)SVG";
    case Symbol::NewDocument:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 4 L14 4 L18 8 L18 20 L7 20 Z"/><path d="M14 4 L14 8 L18 8" stroke-opacity="0.66"/><path d="M12.5 12 L12.5 17" stroke-opacity="0.66"/><path d="M10 14.5 L15 14.5" stroke-opacity="0.66"/></svg>)SVG";
    }
    return {};
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

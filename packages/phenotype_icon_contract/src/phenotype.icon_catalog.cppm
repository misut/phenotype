module;
#include <optional>
#include <string_view>
export module phenotype.icon_catalog;

export namespace phenotype::icon_catalog {

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
    ChevronUp,
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
    AudioDocument,
    CodeDocument,
    SpreadsheetDocument,
    PresentationDocument,
};

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

struct SymbolSourceAttribution {
    std::string_view family;
    std::string_view icon_name;
    std::string_view license;
    std::string_view license_url;
    std::string_view source_url;
    std::string_view source_revision;
    std::string_view copyright;
    bool embedded_source = true;
    bool modified_for_phenotype = false;
    bool apple_asset = false;
    bool platform_extracted = false;
    bool runtime_fetch_required = false;
};

struct IconReferenceSource {
    std::string_view name;
    std::string_view url;
    std::string_view role;
    std::string_view license_policy;
    bool used_as_embedded_asset_source = false;
    bool apple_owned_artwork = false;
};

struct SymbolColor {
    unsigned char r = 96;
    unsigned char g = 96;
    unsigned char b = 100;
    unsigned char a = 255;
};

struct SymbolInteractionState {
    bool selected = false;
    bool enabled = true;
};

enum class SymbolInteractionPhase {
    Normal,
    Hovered,
    Pressed,
};

struct SymbolControlChrome {
    SymbolPresentationRole role = SymbolPresentationRole::Toolbar;
    SymbolTone symbol_tone = SymbolTone::Secondary;
    SymbolColor symbol_color{};
    SymbolColor background_color{0, 0, 0, 0};
    SymbolColor hover_background_color{0, 0, 0, 0};
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
    SymbolColor symbol_color{};
    SymbolColor background_color{0, 0, 0, 0};
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

inline constexpr unsigned int all_symbol_count = 39;
inline constexpr unsigned int phenotype_owned_symbol_count = 4;
inline constexpr unsigned int permissive_source_symbol_count = 35;
inline constexpr unsigned int lucide_source_symbol_count =
    permissive_source_symbol_count;
inline constexpr unsigned int apple_asset_symbol_count = 0;
inline constexpr unsigned int platform_extracted_symbol_count = 0;
inline constexpr unsigned int runtime_fetched_symbol_count = 0;
inline constexpr unsigned int audited_symbol_source_count = all_symbol_count;
inline constexpr unsigned int reference_source_count = 8;
inline constexpr unsigned int sidebar_symbol_count = 11;
inline constexpr unsigned int toolbar_symbol_count = 15;
inline constexpr unsigned int file_type_symbol_count = 11;
inline constexpr unsigned int outline_symbol_count = 38;
inline constexpr unsigned int filled_symbol_count = 1;
inline constexpr unsigned int hierarchical_symbol_count = 17;
inline constexpr unsigned int monochrome_symbol_count = all_symbol_count;
inline constexpr unsigned int regular_weight_symbol_count = all_symbol_count;
inline constexpr unsigned int palette_symbol_count = 0;
inline constexpr unsigned int multicolor_symbol_count = 0;
inline constexpr unsigned int reference_symbol_count = all_symbol_count;
inline constexpr unsigned int svg_path_arc_symbol_count = 16;
inline constexpr unsigned int round_stroke_symbol_count = outline_symbol_count;
inline constexpr unsigned int symbol_interaction_phase_count = 3;

inline auto name(Symbol symbol) noexcept -> std::string_view {
    switch (symbol) {
    case Symbol::Back:         return "back";
    case Symbol::Forward:      return "forward";
    case Symbol::Search:       return "search";
    case Symbol::Share:        return "share";
    case Symbol::Tag:          return "tag";
    case Symbol::More:         return "more";
    case Symbol::Grid:         return "grid";
    case Symbol::List:         return "list";
    case Symbol::Columns:      return "columns";
    case Symbol::Gallery:      return "gallery";
    case Symbol::Folder:       return "folder";
    case Symbol::Trash:        return "trash";
    case Symbol::Document:     return "document";
    case Symbol::Image:        return "image";
    case Symbol::Movie:        return "movie";
    case Symbol::Plus:         return "plus";
    case Symbol::XMark:        return "xmark";
    case Symbol::ChevronDown:  return "chevron_down";
    case Symbol::ChevronUp:    return "chevron_up";
    case Symbol::Home:         return "home";
    case Symbol::Cloud:        return "cloud";
    case Symbol::AirDrop:      return "airdrop";
    case Symbol::Recents:      return "recents";
    case Symbol::Shared:       return "shared";
    case Symbol::Sidebar:      return "sidebar";
    case Symbol::NewFolder:    return "new_folder";
    case Symbol::Applications: return "applications";
    case Symbol::Desktop:      return "desktop";
    case Symbol::Download:     return "download";
    case Symbol::SortGroup:    return "sort_group";
    case Symbol::Duplicate:    return "duplicate";
    case Symbol::NewDocument:  return "new_document";
    case Symbol::PdfDocument:  return "pdf_document";
    case Symbol::TextDocument: return "text_document";
    case Symbol::Archive:      return "archive";
    case Symbol::AudioDocument: return "audio_document";
    case Symbol::CodeDocument: return "code_document";
    case Symbol::SpreadsheetDocument: return "spreadsheet_document";
    case Symbol::PresentationDocument: return "presentation_document";
    }
    return "unknown";
}

inline auto symbol_role_name(SymbolRole role) noexcept -> std::string_view {
    switch (role) {
    case SymbolRole::Navigation: return "navigation";
    case SymbolRole::Toolbar:    return "toolbar";
    case SymbolRole::Sidebar:    return "sidebar";
    case SymbolRole::FileType:   return "file_type";
    case SymbolRole::Action:     return "action";
    }
    return "toolbar";
}

inline auto symbol_variant_name(SymbolVariant variant) noexcept
        -> std::string_view {
    switch (variant) {
    case SymbolVariant::Outline: return "outline";
    case SymbolVariant::Fill:    return "fill";
    }
    return "outline";
}

inline auto symbol_rendering_mode_name(SymbolRenderingMode mode) noexcept
        -> std::string_view {
    switch (mode) {
    case SymbolRenderingMode::Monochrome:   return "monochrome";
    case SymbolRenderingMode::Hierarchical: return "hierarchical";
    }
    return "monochrome";
}

inline auto symbol_scale_name(SymbolScale scale) noexcept -> std::string_view {
    switch (scale) {
    case SymbolScale::Small:  return "small";
    case SymbolScale::Medium: return "medium";
    case SymbolScale::Large:  return "large";
    }
    return "medium";
}

inline auto symbol_weight_name(SymbolWeight weight) noexcept
        -> std::string_view {
    switch (weight) {
    case SymbolWeight::Regular: return "regular";
    }
    return "regular";
}

inline auto symbol_stroke_cap_name(SymbolStrokeCap cap) noexcept
        -> std::string_view {
    switch (cap) {
    case SymbolStrokeCap::Round: return "round";
    }
    return "round";
}

inline auto symbol_stroke_join_name(SymbolStrokeJoin join) noexcept
        -> std::string_view {
    switch (join) {
    case SymbolStrokeJoin::Round: return "round";
    }
    return "round";
}

inline auto symbol_presentation_role_name(
        SymbolPresentationRole role) noexcept -> std::string_view {
    switch (role) {
    case SymbolPresentationRole::Toolbar:    return "toolbar";
    case SymbolPresentationRole::Navigation: return "navigation";
    case SymbolPresentationRole::Sidebar:    return "sidebar";
    case SymbolPresentationRole::FileType:   return "file_type";
    case SymbolPresentationRole::Action:     return "action";
    }
    return "toolbar";
}

inline auto symbol_tone_name(SymbolTone tone) noexcept -> std::string_view {
    switch (tone) {
    case SymbolTone::Primary:     return "primary";
    case SymbolTone::Secondary:   return "secondary";
    case SymbolTone::Selected:    return "selected";
    case SymbolTone::Accent:      return "accent";
    case SymbolTone::Disabled:    return "disabled";
    case SymbolTone::Destructive: return "destructive";
    }
    return "primary";
}

inline auto symbol_interaction_phase_name(
        SymbolInteractionPhase phase) noexcept -> std::string_view {
    switch (phase) {
    case SymbolInteractionPhase::Normal:  return "normal";
    case SymbolInteractionPhase::Hovered: return "hovered";
    case SymbolInteractionPhase::Pressed: return "pressed";
    }
    return "normal";
}

inline auto style_name() noexcept -> std::string_view {
    return "macos_rounded_outline_svg";
}

inline auto style_reference() noexcept -> std::string_view {
    return "Apple HIG, macOS Finder, and SF Symbols inspired custom rounded-outline SVG glyphs";
}

inline auto asset_policy() noexcept -> std::string_view {
    return "phenotype-owned or audited permissive vector assets; no Apple or SF Symbols artwork embedded";
}

inline auto source_license_policy() noexcept -> std::string_view {
    return "phenotype-owned or permissive SVG only; accepted external licenses include Lucide ISC, Feather-derived MIT, Tabler MIT, Iconoir MIT, and Apache-2.0 with per-symbol attribution";
}

inline auto preferred_external_source_policy() noexcept -> std::string_view {
    return "Prefer audited SVG from Lucide ISC or Feather-derived MIT, Tabler MIT, Iconoir MIT, or Material Symbols Apache-2.0 before phenotype adaptation";
}

inline auto source_acquisition_policy() noexcept -> std::string_view {
    return "development-time import from pinned raw permissive SVG URLs only; runtime uses embedded SVG strings and never extracts platform icons, macOS system icons, or fetches remote resources";
}

inline auto lucide_source_revision() noexcept -> std::string_view {
    return "5b40f2c5a76a27eeb81c8f1b1c311121dee45495";
}

inline auto source_attribution_policy() noexcept -> std::string_view {
    return "embedded permissive SVG sources must expose family, icon name, exact license, pinned direct raw SVG URL, source revision, copyright, Apple-asset boundary, platform extraction flag, and runtime fetch flag in debug output";
}

inline auto apple_asset_boundary() noexcept -> std::string_view {
    return "Apple SF Symbols, Finder icons, and macOS system icons are design references only; do not extract or embed Apple-owned artwork without explicit legal clearance";
}

inline auto reference_source_at(unsigned int index) noexcept
        -> IconReferenceSource {
    switch (index) {
    case 0:
        return {
            "Apple Human Interface Guidelines: Icons",
            "https://developer.apple.com/design/human-interface-guidelines/icons",
            "semantic and platform style reference",
            "reference only; do not embed Apple-owned artwork",
            false,
            true,
        };
    case 1:
        return {
            "Apple Human Interface Guidelines: SF Symbols",
            "https://developer.apple.com/design/human-interface-guidelines/sf-symbols",
            "semantic symbol naming and rendering mode reference",
            "reference only; do not embed SF Symbols artwork",
            false,
            true,
        };
    case 2:
        return {
            "W3C SVG 2 Paths",
            "https://www.w3.org/TR/SVG2/paths.html",
            "SVG path command parser reference",
            "open web standard reference",
            false,
            false,
        };
    case 3:
        return {
            "Lucide",
            "https://lucide.dev/",
            "audited permissive SVG source for embedded glyphs",
            "ISC plus Feather-derived MIT license with attribution",
            true,
            false,
        };
    case 4:
        return {
            "Feather Icons",
            "https://github.com/feathericons/feather",
            "MIT license provenance for Feather-derived Lucide symbols",
            "MIT license; referenced for license lineage, not copied directly",
            false,
            false,
        };
    case 5:
        return {
            "Google Material Symbols",
            "https://developers.google.com/fonts/docs/material_symbols",
            "future permissive fallback source candidate",
            "Apache-2.0 license with attribution when embedded",
            false,
            false,
        };
    case 6:
        return {
            "Tabler Icons",
            "https://github.com/tabler/tabler-icons",
            "future permissive fallback source candidate",
            "MIT license with attribution when embedded",
            false,
            false,
        };
    case 7:
        return {
            "Iconoir",
            "https://github.com/iconoir-icons/iconoir",
            "future permissive fallback source candidate",
            "MIT license with attribution when embedded",
            false,
            false,
        };
    }
    return reference_source_at(0);
}

inline auto reference_family() noexcept -> std::string_view {
    return "SF Symbols semantic reference";
}

inline auto reference_policy() noexcept -> std::string_view {
    return "semantic reference only; no Apple or SF Symbols vector artwork; embedded sources are phenotype-owned or audited permissive SVG";
}

inline auto presentation_policy() noexcept -> std::string_view {
    return "macos_role_aware_symbol_presentation";
}

inline auto source_format() noexcept -> std::string_view {
    return "svg";
}

inline auto svg_subset_policy() noexcept -> std::string_view {
    return "bounded_svg_icon_subset";
}

inline auto svg_supported_elements() noexcept -> std::string_view {
    return "svg, g, path, rect, circle, ellipse, line, polyline, polygon";
}

inline auto svg_supported_path_commands() noexcept -> std::string_view {
    return "M L H V Q T C S A Z with absolute and relative variants";
}

inline auto svg_supported_style_attributes() noexcept -> std::string_view {
    return "fill, stroke, color, opacity, fill-opacity, stroke-opacity, stroke-width, stroke-linecap, stroke-linejoin";
}

inline auto svg_arc_policy() noexcept -> std::string_view {
    return "circle elements and isolated circular path A/a preserve native ArcTo; chained or elliptical path A/a lowers to bounded cubic Bezier segments";
}

inline auto stroke_geometry_policy() noexcept -> std::string_view {
    return "round_cap_round_join_svg_strokes";
}

inline auto stroke_cap_policy() noexcept -> std::string_view {
    return "round";
}

inline auto stroke_join_policy() noexcept -> std::string_view {
    return "round";
}

inline auto alignment_policy() noexcept -> std::string_view {
    return "24x24 text-aligned symbol grid";
}

inline auto variant_policy() noexcept -> std::string_view {
    return "outline primary with filled action variants";
}

inline auto tone_policy() noexcept -> std::string_view {
    return "primary, secondary, selected, accent, disabled, destructive";
}

inline auto interaction_tone_policy() noexcept -> std::string_view {
    return "macos_finder_interaction_tones";
}

inline auto file_type_color_policy() noexcept -> std::string_view {
    return "macos_finder_file_type_tints";
}

inline auto interface_metaphor_policy() noexcept -> std::string_view {
    return "familiar_simplified_macos_symbol_metaphors";
}

inline auto visual_consistency_policy() noexcept -> std::string_view {
    return "consistent_size_stroke_detail_and_perspective";
}

inline auto toolbar_symbol_chrome_policy() noexcept -> std::string_view {
    return "borderless_toolbar_symbols_inside_grouped_controls";
}

inline auto sidebar_symbol_color_policy() noexcept -> std::string_view {
    return "accent_selected_user_tint_compatible_sidebar_symbols";
}

inline auto symbol_control_chrome_policy() noexcept -> std::string_view {
    return "macos_finder_symbol_state_chrome";
}

inline auto symbol_interaction_phase_policy() noexcept -> std::string_view {
    return "macos_finder_normal_hover_pressed_symbol_chrome";
}

inline auto default_scale_policy() noexcept -> std::string_view {
    return "medium";
}

inline auto default_weight_policy() noexcept -> std::string_view {
    return "regular_text_weight_aligned";
}

inline auto rendering_capability_policy() noexcept -> std::string_view {
    return "sf_symbols_mode_names_with_macos_monochrome_hierarchical_contract";
}

inline auto metrics_policy() noexcept -> std::string_view {
    return "macos_finder_role_metrics_with_explicit_hit_targets";
}

inline auto hit_target_policy() noexcept -> std::string_view {
    return "visual toolbar/navigation/action=36pt, sidebar=38pt, "
           "file_type=64pt; activation minimum=44pt for controls";
}

inline auto symbol_at(unsigned int index) noexcept -> Symbol {
    switch (index) {
    case 0:  return Symbol::Back;
    case 1:  return Symbol::Forward;
    case 2:  return Symbol::Search;
    case 3:  return Symbol::Share;
    case 4:  return Symbol::Tag;
    case 5:  return Symbol::More;
    case 6:  return Symbol::Grid;
    case 7:  return Symbol::List;
    case 8:  return Symbol::Columns;
    case 9:  return Symbol::Gallery;
    case 10: return Symbol::Folder;
    case 11: return Symbol::Trash;
    case 12: return Symbol::Document;
    case 13: return Symbol::Image;
    case 14: return Symbol::Movie;
    case 15: return Symbol::Plus;
    case 16: return Symbol::XMark;
    case 17: return Symbol::ChevronDown;
    case 18: return Symbol::ChevronUp;
    case 19: return Symbol::Home;
    case 20: return Symbol::Cloud;
    case 21: return Symbol::AirDrop;
    case 22: return Symbol::Recents;
    case 23: return Symbol::Shared;
    case 24: return Symbol::Sidebar;
    case 25: return Symbol::NewFolder;
    case 26: return Symbol::Applications;
    case 27: return Symbol::Desktop;
    case 28: return Symbol::Download;
    case 29: return Symbol::SortGroup;
    case 30: return Symbol::Duplicate;
    case 31: return Symbol::NewDocument;
    case 32: return Symbol::PdfDocument;
    case 33: return Symbol::TextDocument;
    case 34: return Symbol::Archive;
    case 35: return Symbol::AudioDocument;
    case 36: return Symbol::CodeDocument;
    case 37: return Symbol::SpreadsheetDocument;
    case 38: return Symbol::PresentationDocument;
    }
    return Symbol::Document;
}

inline auto sidebar_symbol_at(unsigned int index) noexcept -> Symbol {
    switch (index) {
    case 0:  return Symbol::Recents;
    case 1:  return Symbol::Shared;
    case 2:  return Symbol::Applications;
    case 3:  return Symbol::Desktop;
    case 4:  return Symbol::Document;
    case 5:  return Symbol::Download;
    case 6:  return Symbol::Cloud;
    case 7:  return Symbol::Home;
    case 8:  return Symbol::AirDrop;
    case 9:  return Symbol::Trash;
    case 10: return Symbol::Folder;
    }
    return Symbol::Folder;
}

inline auto toolbar_symbol_at(unsigned int index) noexcept -> Symbol {
    switch (index) {
    case 0:  return Symbol::Back;
    case 1:  return Symbol::Forward;
    case 2:  return Symbol::Grid;
    case 3:  return Symbol::List;
    case 4:  return Symbol::Columns;
    case 5:  return Symbol::Gallery;
    case 6:  return Symbol::SortGroup;
    case 7:  return Symbol::Share;
    case 8:  return Symbol::Tag;
    case 9:  return Symbol::More;
    case 10: return Symbol::Search;
    case 11: return Symbol::NewDocument;
    case 12: return Symbol::NewFolder;
    case 13: return Symbol::Duplicate;
    case 14: return Symbol::Trash;
    }
    return Symbol::Search;
}

inline auto file_type_symbol_at(unsigned int index) noexcept -> Symbol {
    switch (index) {
    case 0: return Symbol::Folder;
    case 1: return Symbol::Document;
    case 2: return Symbol::PdfDocument;
    case 3: return Symbol::TextDocument;
    case 4: return Symbol::Image;
    case 5: return Symbol::Movie;
    case 6: return Symbol::Archive;
    case 7: return Symbol::AudioDocument;
    case 8: return Symbol::CodeDocument;
    case 9: return Symbol::SpreadsheetDocument;
    case 10: return Symbol::PresentationDocument;
    }
    return Symbol::Document;
}

inline auto symbol_from_name(std::string_view symbol_name) noexcept
        -> std::optional<Symbol> {
    for (unsigned int i = 0; i < all_symbol_count; ++i) {
        auto const symbol = symbol_at(i);
        if (name(symbol) == symbol_name)
            return symbol;
    }
    return std::nullopt;
}

inline auto semantic_reference_name(Symbol symbol) noexcept
        -> std::string_view {
    switch (symbol) {
    case Symbol::Back:         return "chevron.left";
    case Symbol::Forward:      return "chevron.right";
    case Symbol::Search:       return "magnifyingglass";
    case Symbol::Share:        return "square.and.arrow.up";
    case Symbol::Tag:          return "tag";
    case Symbol::More:         return "ellipsis";
    case Symbol::Grid:         return "square.grid.2x2";
    case Symbol::List:         return "list.bullet";
    case Symbol::Columns:      return "rectangle.split.3x1";
    case Symbol::Gallery:      return "rectangle.on.rectangle";
    case Symbol::Folder:       return "folder";
    case Symbol::Trash:        return "trash";
    case Symbol::Document:     return "doc";
    case Symbol::Image:        return "photo";
    case Symbol::Movie:        return "film";
    case Symbol::Plus:         return "plus";
    case Symbol::XMark:        return "xmark";
    case Symbol::ChevronDown:  return "chevron.down";
    case Symbol::ChevronUp:    return "chevron.up";
    case Symbol::Home:         return "house";
    case Symbol::Cloud:        return "icloud";
    case Symbol::AirDrop:      return "airdrop";
    case Symbol::Recents:      return "clock";
    case Symbol::Shared:       return "folder.badge.person.crop";
    case Symbol::Sidebar:      return "sidebar.left";
    case Symbol::NewFolder:    return "folder.badge.plus";
    case Symbol::Applications: return "app";
    case Symbol::Desktop:      return "desktopcomputer";
    case Symbol::Download:     return "arrow.down.circle";
    case Symbol::SortGroup:    return "rectangle.grid.3x2";
    case Symbol::Duplicate:    return "square.on.square";
    case Symbol::NewDocument:  return "doc.badge.plus";
    case Symbol::PdfDocument:  return "doc.richtext";
    case Symbol::TextDocument: return "doc.plaintext";
    case Symbol::Archive:      return "archivebox";
    case Symbol::AudioDocument: return "music.note";
    case Symbol::CodeDocument: return "chevron.left.forwardslash.chevron.right";
    case Symbol::SpreadsheetDocument: return "tablecells";
    case Symbol::PresentationDocument: return "rectangle.on.rectangle.angled";
    }
    return "doc";
}

inline auto symbol_from_semantic_reference_name(
        std::string_view reference_name) noexcept -> std::optional<Symbol> {
    for (unsigned int i = 0; i < all_symbol_count; ++i) {
        auto const symbol = symbol_at(i);
        if (semantic_reference_name(symbol) == reference_name)
            return symbol;
    }
    return std::nullopt;
}

inline bool uses_lucide_source(Symbol symbol) noexcept {
    switch (symbol) {
    case Symbol::Back:
    case Symbol::Forward:
    case Symbol::Search:
    case Symbol::Share:
    case Symbol::Tag:
    case Symbol::Grid:
    case Symbol::List:
    case Symbol::Columns:
    case Symbol::Gallery:
    case Symbol::Folder:
    case Symbol::Trash:
    case Symbol::Document:
    case Symbol::Image:
    case Symbol::Movie:
    case Symbol::Plus:
    case Symbol::XMark:
    case Symbol::ChevronDown:
    case Symbol::ChevronUp:
    case Symbol::Home:
    case Symbol::Cloud:
    case Symbol::Recents:
    case Symbol::Sidebar:
    case Symbol::NewFolder:
    case Symbol::Applications:
    case Symbol::Desktop:
    case Symbol::Download:
    case Symbol::Duplicate:
    case Symbol::NewDocument:
    case Symbol::PdfDocument:
    case Symbol::TextDocument:
    case Symbol::Archive:
    case Symbol::AudioDocument:
    case Symbol::CodeDocument:
    case Symbol::SpreadsheetDocument:
    case Symbol::PresentationDocument:
        return true;
    default:
        return false;
    }
}

inline auto permissive_source_icon_name(Symbol symbol) noexcept
        -> std::string_view {
    switch (symbol) {
    case Symbol::Back:         return "chevron-left";
    case Symbol::Forward:      return "chevron-right";
    case Symbol::Search:       return "search";
    case Symbol::Share:        return "share";
    case Symbol::Tag:          return "tag";
    case Symbol::Grid:         return "grid-2x2";
    case Symbol::List:         return "list";
    case Symbol::Columns:      return "columns-3";
    case Symbol::Gallery:      return "gallery-horizontal";
    case Symbol::Folder:       return "folder";
    case Symbol::Trash:        return "trash-2";
    case Symbol::Document:     return "file";
    case Symbol::Image:        return "file-image";
    case Symbol::Movie:        return "clapperboard";
    case Symbol::Plus:         return "plus";
    case Symbol::XMark:        return "x";
    case Symbol::ChevronDown:  return "chevron-down";
    case Symbol::ChevronUp:    return "chevron-up";
    case Symbol::Home:         return "house";
    case Symbol::Cloud:        return "cloud";
    case Symbol::Recents:      return "clock";
    case Symbol::Sidebar:      return "panel-left";
    case Symbol::NewFolder:    return "folder-plus";
    case Symbol::Applications: return "app-window";
    case Symbol::Desktop:      return "monitor";
    case Symbol::Download:     return "circle-arrow-down";
    case Symbol::Duplicate:    return "copy";
    case Symbol::NewDocument:  return "file-plus";
    case Symbol::PdfDocument:  return "file-text";
    case Symbol::TextDocument: return "file-text";
    case Symbol::Archive:      return "file-archive";
    case Symbol::AudioDocument: return "file-music";
    case Symbol::CodeDocument: return "file-code";
    case Symbol::SpreadsheetDocument: return "file-spreadsheet";
    case Symbol::PresentationDocument: return "presentation";
    default:                   return name(symbol);
    }
}

inline auto lucide_source_url(std::string_view icon_name) noexcept
        -> std::string_view {
    if (icon_name == "chevron-left")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/chevron-left.svg";
    if (icon_name == "chevron-right")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/chevron-right.svg";
    if (icon_name == "search")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/search.svg";
    if (icon_name == "share")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/share.svg";
    if (icon_name == "tag")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/tag.svg";
    if (icon_name == "grid-2x2")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/grid-2x2.svg";
    if (icon_name == "list")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/list.svg";
    if (icon_name == "columns-3")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/columns-3.svg";
    if (icon_name == "gallery-horizontal")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/gallery-horizontal.svg";
    if (icon_name == "folder")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/folder.svg";
    if (icon_name == "trash-2")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/trash-2.svg";
    if (icon_name == "file")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/file.svg";
    if (icon_name == "file-image")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/file-image.svg";
    if (icon_name == "clapperboard")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/clapperboard.svg";
    if (icon_name == "plus")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/plus.svg";
    if (icon_name == "x")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/x.svg";
    if (icon_name == "chevron-down")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/chevron-down.svg";
    if (icon_name == "chevron-up")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/chevron-up.svg";
    if (icon_name == "house")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/house.svg";
    if (icon_name == "cloud")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/cloud.svg";
    if (icon_name == "clock")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/clock.svg";
    if (icon_name == "panel-left")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/panel-left.svg";
    if (icon_name == "folder-plus")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/folder-plus.svg";
    if (icon_name == "app-window")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/app-window.svg";
    if (icon_name == "monitor")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/monitor.svg";
    if (icon_name == "circle-arrow-down")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/circle-arrow-down.svg";
    if (icon_name == "copy")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/copy.svg";
    if (icon_name == "file-plus")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/file-plus.svg";
    if (icon_name == "file-text")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/file-text.svg";
    if (icon_name == "file-archive")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/file-archive.svg";
    if (icon_name == "file-music")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/file-music.svg";
    if (icon_name == "file-code")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/file-code.svg";
    if (icon_name == "file-spreadsheet")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/file-spreadsheet.svg";
    if (icon_name == "presentation")
        return "https://raw.githubusercontent.com/lucide-icons/lucide/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons/presentation.svg";
    return "https://github.com/lucide-icons/lucide/tree/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/icons";
}

inline auto lucide_license_url() noexcept -> std::string_view {
    return "https://github.com/lucide-icons/lucide/blob/5b40f2c5a76a27eeb81c8f1b1c311121dee45495/LICENSE";
}

inline bool lucide_icon_uses_feather_mit(std::string_view icon_name) noexcept {
    return icon_name == "chevron-left"
        || icon_name == "chevron-right"
        || icon_name == "chevron-down"
        || icon_name == "chevron-up"
        || icon_name == "clock"
        || icon_name == "monitor"
        || icon_name == "plus"
        || icon_name == "search"
        || icon_name == "share"
        || icon_name == "trash-2"
        || icon_name == "x";
}

inline auto lucide_icon_license(std::string_view icon_name) noexcept
        -> std::string_view {
    return lucide_icon_uses_feather_mit(icon_name) ? "MIT" : "ISC";
}

inline auto lucide_icon_copyright(std::string_view icon_name) noexcept
        -> std::string_view {
    if (lucide_icon_uses_feather_mit(icon_name))
        return "Copyright (c) 2013-present Cole Bemis; Lucide modifications copyright (c) 2026 Lucide Icons and Contributors";
    return "Copyright (c) 2026 Lucide Icons and Contributors";
}

inline auto source_attribution(Symbol symbol) noexcept
        -> SymbolSourceAttribution {
    if (uses_lucide_source(symbol)) {
        auto const icon = permissive_source_icon_name(symbol);
        return SymbolSourceAttribution{
            "Lucide",
            icon,
            lucide_icon_license(icon),
            lucide_license_url(),
            lucide_source_url(icon),
            lucide_source_revision(),
            lucide_icon_copyright(icon),
            true,
            true,
            false,
            false,
            false,
        };
    }
    return SymbolSourceAttribution{
        "phenotype",
        name(symbol),
        "MIT",
        "https://github.com/misut/phenotype/blob/main/LICENSE",
        "built-in phenotype.icon_catalog",
        "phenotype-repository",
        "Copyright (c) misut",
        true,
        false,
        false,
        false,
        false,
    };
}

inline auto lucide_svg_source(Symbol symbol) noexcept -> std::string_view {
    switch (symbol) {
    case Symbol::Back:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m15 18-6-6 6-6"/></svg>)SVG";
    case Symbol::Forward:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m9 18 6-6-6-6"/></svg>)SVG";
    case Symbol::Search:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m21 21-4.34-4.34"/><circle cx="11" cy="11" r="8"/></svg>)SVG";
    case Symbol::Share:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2v13"/><path d="m16 6-4-4-4 4"/><path d="M4 12v8a2 2 0 0 0 2 2h12a2 2 0 0 0 2-2v-8" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Tag:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12.586 2.586A2 2 0 0 0 11.172 2H4a2 2 0 0 0-2 2v7.172a2 2 0 0 0 .586 1.414l8.704 8.704a2.426 2.426 0 0 0 3.42 0l6.58-6.58a2.426 2.426 0 0 0 0-3.42z"/><circle cx="7.5" cy="7.5" r=".5" fill="currentColor" fill-opacity="0.66"/></svg>)SVG";
    case Symbol::Grid:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3v18"/><path d="M3 12h18"/><rect x="3" y="3" width="18" height="18" rx="2"/></svg>)SVG";
    case Symbol::List:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M3 5h.01" stroke-opacity="0.66"/><path d="M3 12h.01" stroke-opacity="0.66"/><path d="M3 19h.01" stroke-opacity="0.66"/><path d="M8 5h13"/><path d="M8 12h13"/><path d="M8 19h13"/></svg>)SVG";
    case Symbol::Columns:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect width="18" height="18" x="3" y="3" rx="2"/><path d="M9 3v18" stroke-opacity="0.66"/><path d="M15 3v18" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Gallery:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 3v18" stroke-opacity="0.66"/><rect width="12" height="18" x="6" y="3" rx="2"/><path d="M22 3v18" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Folder:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z"/></svg>)SVG";
    case Symbol::Trash:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M10 11v6" stroke-opacity="0.66"/><path d="M14 11v6" stroke-opacity="0.66"/><path d="M19 6v14a2 2 0 0 1-2 2H7a2 2 0 0 1-2-2V6"/><path d="M3 6h18"/><path d="M8 6V4a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Document:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/></svg>)SVG";
    case Symbol::Image:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><circle cx="10" cy="12" r="2"/><path d="m20 17-1.296-1.296a2.41 2.41 0 0 0-3.408 0L9 22"/></svg>)SVG";
    case Symbol::Movie:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m12.296 3.464 3.02 3.956"/><path d="M20.2 6 3 11l-.9-2.4c-.3-1.1.3-2.2 1.3-2.5l13.5-4c1.1-.3 2.2.3 2.5 1.3z"/><path d="M3 11h18v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><path d="m6.18 5.276 3.1 3.899"/></svg>)SVG";
    case Symbol::Plus:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M5 12h14"/><path d="M12 5v14"/></svg>)SVG";
    case Symbol::XMark:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M18 6 6 18"/><path d="m6 6 12 12"/></svg>)SVG";
    case Symbol::ChevronDown:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m6 9 6 6 6-6"/></svg>)SVG";
    case Symbol::ChevronUp:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m18 15-6-6-6 6"/></svg>)SVG";
    case Symbol::Home:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M15 21v-8a1 1 0 0 0-1-1h-4a1 1 0 0 0-1 1v8" stroke-opacity="0.66"/><path d="M3 10a2 2 0 0 1 .709-1.528l7-6a2 2 0 0 1 2.582 0l7 6A2 2 0 0 1 21 10v9a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/></svg>)SVG";
    case Symbol::Cloud:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M17.5 19H9a7 7 0 1 1 6.71-9h1.79a4.5 4.5 0 1 1 0 9Z"/></svg>)SVG";
    case Symbol::Recents:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><path d="M12 6v6l4 2" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Sidebar:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect width="18" height="18" x="3" y="3" rx="2"/><path d="M9 3v18" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::NewFolder:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 10v6" stroke-opacity="0.66"/><path d="M9 13h6" stroke-opacity="0.66"/><path d="M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z"/></svg>)SVG";
    case Symbol::Applications:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="2" y="4" width="20" height="16" rx="2"/><path d="M10 4v4" stroke-opacity="0.66"/><path d="M2 8h20" stroke-opacity="0.66"/><path d="M6 4v4" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Desktop:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect width="20" height="14" x="2" y="3" rx="2"/><line x1="8" x2="16" y1="21" y2="21" stroke-opacity="0.66"/><line x1="12" x2="12" y1="17" y2="21" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Download:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="10"/><path d="M12 8v8"/><path d="m8 12 4 4 4-4"/></svg>)SVG";
    case Symbol::Duplicate:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect width="14" height="14" x="8" y="8" rx="2" ry="2"/><path d="M4 16c-1.1 0-2-.9-2-2V4c0-1.1.9-2 2-2h10c1.1 0 2 .9 2 2" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::NewDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M9 15h6" stroke-opacity="0.66"/><path d="M12 18v-6" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::PdfDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M10 9H8"/><path d="M16 13H8"/><path d="M16 17H8"/></svg>)SVG";
    case Symbol::TextDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M10 9H8"/><path d="M16 13H8"/><path d="M16 17H8"/></svg>)SVG";
    case Symbol::Archive:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M13.659 22H18a2 2 0 0 0 2-2V8a2.4 2.4 0 0 0-.706-1.706l-3.588-3.588A2.4 2.4 0 0 0 14 2H6a2 2 0 0 0-2 2v11.5"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M8 12v-1"/><path d="M8 18v-2"/><path d="M8 7V6"/><circle cx="8" cy="20" r="2"/></svg>)SVG";
    case Symbol::AudioDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M11.65 22H18a2 2 0 0 0 2-2V8a2.4 2.4 0 0 0-.706-1.706l-3.588-3.588A2.4 2.4 0 0 0 14 2H6a2 2 0 0 0-2 2v10.35"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M8 20v-7l3 1.474"/><circle cx="6" cy="20" r="2"/></svg>)SVG";
    case Symbol::CodeDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M10 12.5 8 15l2 2.5"/><path d="m14 12.5 2 2.5-2 2.5"/></svg>)SVG";
    case Symbol::SpreadsheetDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M8 13h2"/><path d="M14 13h2"/><path d="M8 17h2"/><path d="M14 17h2"/></svg>)SVG";
    case Symbol::PresentationDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M2 3h20"/><path d="M21 3v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V3"/><path d="m7 21 5-5 5 5"/></svg>)SVG";
    default:
        return {};
    }
}

inline auto svg_source(Symbol symbol) noexcept -> std::string_view {
    if (uses_lucide_source(symbol))
        return lucide_svg_source(symbol);

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
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4.9 12.5 Q4.3 11.9 4.9 11.3 L11.3 4.9 Q11.9 4.3 12.5 4.9 L19.1 11.5 Q19.7 12.1 19.1 12.7 L12.7 19.1 Q12.1 19.7 11.5 19.1 Z"/><circle cx="12.2" cy="8.9" r="1.15" stroke-opacity="0.66"/></svg>)SVG";
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
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M20 20a2 2 0 0 0 2-2V8a2 2 0 0 0-2-2h-7.9a2 2 0 0 1-1.69-.9L9.6 3.9A2 2 0 0 0 7.93 3H4a2 2 0 0 0-2 2v13a2 2 0 0 0 2 2Z"/></svg>)SVG";
    case Symbol::Trash:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 8 L17 8"/><path d="M10 5 L14 5 L15 8"/><path d="M8 8 L9 20 L15 20 L16 8"/><path d="M11 11 L11 17" stroke-opacity="0.66"/><path d="M13 11 L13 17" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Document:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/></svg>)SVG";
    case Symbol::Image:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><circle cx="10" cy="12" r="2"/><path d="m20 17-1.296-1.296a2.41 2.41 0 0 0-3.408 0L9 22"/></svg>)SVG";
    case Symbol::Movie:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="m12.296 3.464 3.02 3.956"/><path d="M20.2 6 3 11l-.9-2.4c-.3-1.1.3-2.2 1.3-2.5l13.5-4c1.1-.3 2.2.3 2.5 1.3z"/><path d="M3 11h18v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/><path d="m6.18 5.276 3.1 3.899"/></svg>)SVG";
    case Symbol::Plus:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 5 L12 19"/><path d="M5 12 L19 12"/></svg>)SVG";
    case Symbol::XMark:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6.5 6.5 L17.5 17.5"/><path d="M17.5 6.5 L6.5 17.5"/></svg>)SVG";
    case Symbol::ChevronDown:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6.5 9 L12 14.5 L17.5 9"/></svg>)SVG";
    case Symbol::ChevronUp:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6.5 15 L12 9.5 L17.5 15"/></svg>)SVG";
    case Symbol::Home:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M4 11.5 L12 5 L20 11.5"/><path d="M6.5 10 L6.5 20 L17.5 20 L17.5 10"/><path d="M10 20 L10 14 L14 14 L14 20" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Cloud:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 18 L17 18 C20 18 21.5 16 21.5 13.7 C21.5 11.5 19.8 9.8 17.6 9.7 C16.8 7.3 14.7 6 12.2 6 C9.4 6 7.3 7.8 6.7 10.3 C4.4 10.7 2.5 12.2 2.5 14.4 C2.5 16.4 4.1 18 7 18 Z"/></svg>)SVG";
    case Symbol::AirDrop:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="1.2"/><path d="M8.1 14.1 A4.4 4.4 0 0 0 15.9 14.1" stroke-opacity="0.66"/><path d="M5.4 15.2 A7.2 7.2 0 0 0 18.6 15.2" stroke-opacity="0.48"/><path d="M3.4 16.8 A9.5 9.5 0 0 0 20.6 16.8" stroke-opacity="0.34"/><path d="M12 13.5 L12 19.2" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Recents:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="8"/><path d="M12 7 L12 12 L8.5 12" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Shared:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M3.9 9.1 Q3.9 8.1 4.9 8.1 L8.7 8.1 Q9.3 8.1 9.8 8.6 L10.9 9.7 L18.4 9.7 Q19.4 9.7 19.4 10.7 L19.4 18 Q19.4 19 18.4 19 L4.9 19 Q3.9 19 3.9 18 Z"/><circle cx="17.2" cy="7.4" r="2.05" stroke-opacity="0.66"/><path d="M13.8 12.4 C14.5 11.1 15.7 10.5 17.2 10.5 C18.7 10.5 20 11.1 20.6 12.4" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Sidebar:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="14" rx="2.4"/><path d="M9 5 L9 19" stroke-opacity="0.66"/><path d="M6.2 8 L7.1 8" stroke-opacity="0.66"/><path d="M6.2 11 L7.1 11" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::NewFolder:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M4.2 9.2 Q4.2 8.2 5.2 8.2 L9 8.2 Q9.7 8.2 10.2 8.7 L11.4 9.8 L18.8 9.8 Q19.8 9.8 19.8 10.8 L19.8 18.2 Q19.8 19.2 18.8 19.2 L5.2 19.2 Q4.2 19.2 4.2 18.2 Z"/><path d="M14 13.1 L14 17" stroke-opacity="0.66"/><path d="M12.1 15 L15.9 15" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Applications:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.75" stroke-linecap="round" stroke-linejoin="round"><path d="M6.4 19 L12.1 5.2"/><path d="M17.6 19 L11.9 5.2"/><path d="M8.2 14.5 L15.8 14.5" stroke-opacity="0.66"/><path d="M5 19.2 C7 18.7 9.3 18.5 12 18.5 C14.7 18.5 17 18.7 19 19.2" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Desktop:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="12" rx="2.4"/><path d="M9 20 L15 20" stroke-opacity="0.66"/><path d="M7 21.5 L17 21.5" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Download:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="8"/><path d="M12 7 L12 15"/><path d="M8.5 12 L12 15.5 L15.5 12"/></svg>)SVG";
    case Symbol::SortGroup:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="4.2" height="3" rx="0.8"/><rect x="10" y="5" width="4.2" height="3" rx="0.8"/><rect x="16" y="5" width="4.2" height="3" rx="0.8"/><rect x="4" y="11" width="4.2" height="3" rx="0.8"/><rect x="10" y="11" width="4.2" height="3" rx="0.8"/><rect x="16" y="11" width="4.2" height="3" rx="0.8"/><rect x="4" y="17" width="4.2" height="3" rx="0.8"/><rect x="10" y="17" width="4.2" height="3" rx="0.8"/><path d="M17 16 L19.2 18.2 L21.4 16" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::Duplicate:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="6" y="8" width="10" height="12" rx="2" stroke-opacity="0.66"/><rect x="10" y="4" width="10" height="12" rx="2"/></svg>)SVG";
    case Symbol::NewDocument:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7.2 4.2 L13.7 4.2 Q14.3 4.2 14.8 4.7 L17.5 7.4 Q18 7.9 18 8.6 L18 18.9 Q18 19.8 17.1 19.8 L7.2 19.8 Q6.3 19.8 6.3 18.9 L6.3 5.1 Q6.3 4.2 7.2 4.2 Z"/><path d="M14.2 4.5 L14.2 8.1 L17.8 8.1" stroke-opacity="0.66"/><path d="M12.4 12.2 L12.4 17" stroke-opacity="0.66"/><path d="M10 14.6 L14.8 14.6" stroke-opacity="0.66"/></svg>)SVG";
    case Symbol::PdfDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M10 9H8"/><path d="M16 13H8"/><path d="M16 17H8"/></svg>)SVG";
    case Symbol::TextDocument:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6 22a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h8a2.4 2.4 0 0 1 1.704.706l3.588 3.588A2.4 2.4 0 0 1 20 8v12a2 2 0 0 1-2 2z"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M10 9H8"/><path d="M16 13H8"/><path d="M16 17H8"/></svg>)SVG";
    case Symbol::Archive:
        return R"SVG(<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M13.659 22H18a2 2 0 0 0 2-2V8a2.4 2.4 0 0 0-.706-1.706l-3.588-3.588A2.4 2.4 0 0 0 14 2H6a2 2 0 0 0-2 2v11.5"/><path d="M14 2v5a1 1 0 0 0 1 1h5"/><path d="M8 12v-1"/><path d="M8 18v-2"/><path d="M8 7V6"/><circle cx="8" cy="20" r="2"/></svg>)SVG";
    }
    return {};
}

inline bool supports_hierarchical_opacity(Symbol symbol) noexcept {
    switch (symbol) {
    case Symbol::Share:
    case Symbol::Tag:
    case Symbol::List:
    case Symbol::Columns:
    case Symbol::Gallery:
    case Symbol::Trash:
    case Symbol::Home:
    case Symbol::AirDrop:
    case Symbol::Recents:
    case Symbol::Shared:
    case Symbol::Sidebar:
    case Symbol::NewFolder:
    case Symbol::Applications:
    case Symbol::Desktop:
    case Symbol::SortGroup:
    case Symbol::Duplicate:
    case Symbol::NewDocument:
        return true;
    default:
        return false;
    }
}

inline auto symbol_layer_count(Symbol symbol) noexcept -> unsigned int {
    switch (symbol) {
    case Symbol::More:        return 3;
    case Symbol::Grid:        return 3;
    case Symbol::List:        return 6;
    case Symbol::Trash:       return 5;
    case Symbol::Movie:       return 4;
    case Symbol::PdfDocument: return 5;
    case Symbol::TextDocument: return 5;
    case Symbol::Archive:     return 6;
    case Symbol::AudioDocument: return 4;
    case Symbol::CodeDocument: return 4;
    case Symbol::SpreadsheetDocument: return 6;
    case Symbol::PresentationDocument: return 3;
    case Symbol::Sidebar:     return 2;
    case Symbol::SortGroup:   return 9;
    case Symbol::AirDrop:     return 5;
    case Symbol::Applications:
    case Symbol::NewDocument:
        return 4;
    case Symbol::Document:
        return 2;
    case Symbol::Columns:
    case Symbol::Gallery:
    case Symbol::Share:
    case Symbol::Shared:
    case Symbol::NewFolder:
    case Symbol::Desktop:
    case Symbol::Download:
        return 3;
    case Symbol::Home:
        return 2;
    case Symbol::Image:
        return 4;
    case Symbol::Search:
    case Symbol::Tag:
    case Symbol::Plus:
    case Symbol::XMark:
    case Symbol::Recents:
    case Symbol::Duplicate:
        return 2;
    default:
        return 1;
    }
}

inline bool uses_svg_path_arcs(Symbol symbol) noexcept {
    return symbol == Symbol::AirDrop
        || symbol == Symbol::Tag
        || symbol == Symbol::Folder
        || symbol == Symbol::Trash
        || symbol == Symbol::Document
        || symbol == Symbol::Image
        || symbol == Symbol::Home
        || symbol == Symbol::Cloud
        || symbol == Symbol::NewFolder
        || symbol == Symbol::NewDocument
        || symbol == Symbol::PdfDocument
        || symbol == Symbol::TextDocument
        || symbol == Symbol::Archive
        || symbol == Symbol::AudioDocument
        || symbol == Symbol::CodeDocument
        || symbol == Symbol::SpreadsheetDocument;
}

inline auto rendering_capabilities(Symbol symbol) noexcept
        -> SymbolRenderingCapabilities {
    return {
        true,
        supports_hierarchical_opacity(symbol),
        false,
        false,
        rendering_capability_policy(),
    };
}

inline auto descriptor(Symbol symbol) noexcept -> SymbolDescriptor {
    SymbolRole role = SymbolRole::Toolbar;
    switch (symbol) {
    case Symbol::Back:
    case Symbol::Forward:
    case Symbol::ChevronDown:
    case Symbol::ChevronUp:
        role = SymbolRole::Navigation;
        break;
    case Symbol::Folder:
    case Symbol::Document:
    case Symbol::Image:
    case Symbol::Movie:
    case Symbol::PdfDocument:
    case Symbol::TextDocument:
    case Symbol::Archive:
    case Symbol::AudioDocument:
    case Symbol::CodeDocument:
    case Symbol::SpreadsheetDocument:
    case Symbol::PresentationDocument:
        role = SymbolRole::FileType;
        break;
    case Symbol::Home:
    case Symbol::Cloud:
    case Symbol::AirDrop:
    case Symbol::Recents:
    case Symbol::Shared:
    case Symbol::Sidebar:
    case Symbol::Applications:
    case Symbol::Desktop:
    case Symbol::Download:
        role = SymbolRole::Sidebar;
        break;
    case Symbol::Plus:
    case Symbol::XMark:
    case Symbol::NewFolder:
    case Symbol::Duplicate:
    case Symbol::NewDocument:
    case Symbol::Trash:
        role = SymbolRole::Action;
        break;
    case Symbol::Search:
    case Symbol::Share:
    case Symbol::Tag:
    case Symbol::More:
    case Symbol::Grid:
    case Symbol::List:
    case Symbol::Columns:
    case Symbol::Gallery:
    case Symbol::SortGroup:
        role = SymbolRole::Toolbar;
        break;
    }

    bool const filled = symbol == Symbol::More;
    bool const hierarchical = supports_hierarchical_opacity(symbol);
    float const stroke_width = filled
        ? 0.0f
        : (uses_lucide_source(symbol) ? 2.0f : 1.8f);
    return SymbolDescriptor{
        symbol,
        name(symbol),
        role,
        filled ? SymbolVariant::Fill : SymbolVariant::Outline,
        hierarchical
            ? SymbolRenderingMode::Hierarchical
            : SymbolRenderingMode::Monochrome,
        SymbolScale::Medium,
        SymbolWeight::Regular,
        style_name(),
        semantic_reference_name(symbol),
        reference_family(),
        reference_policy(),
        24.0f,
        stroke_width,
        SymbolStrokeCap::Round,
        SymbolStrokeJoin::Round,
        hierarchical ? 0.66f : 1.0f,
        symbol_layer_count(symbol),
        true,
        !filled,
        filled,
        true,
        true,
        hierarchical,
        false,
        false,
        !uses_lucide_source(symbol),
        false,
    };
}

inline auto default_presentation_role(Symbol symbol) noexcept
        -> SymbolPresentationRole {
    switch (descriptor(symbol).role) {
    case SymbolRole::Navigation: return SymbolPresentationRole::Navigation;
    case SymbolRole::Toolbar:    return SymbolPresentationRole::Toolbar;
    case SymbolRole::Sidebar:    return SymbolPresentationRole::Sidebar;
    case SymbolRole::FileType:   return SymbolPresentationRole::FileType;
    case SymbolRole::Action:     return SymbolPresentationRole::Action;
    }
    return SymbolPresentationRole::Toolbar;
}

inline auto default_scale(SymbolPresentationRole role) noexcept
        -> SymbolScale {
    switch (role) {
    case SymbolPresentationRole::Sidebar:
    case SymbolPresentationRole::FileType:
        return SymbolScale::Large;
    case SymbolPresentationRole::Toolbar:
    case SymbolPresentationRole::Navigation:
    case SymbolPresentationRole::Action:
        return SymbolScale::Medium;
    }
    return SymbolScale::Medium;
}

inline float point_size(SymbolScale scale) noexcept {
    switch (scale) {
    case SymbolScale::Small:  return 20.0f;
    case SymbolScale::Medium: return 24.0f;
    case SymbolScale::Large:  return 26.0f;
    }
    return 24.0f;
}

inline float hit_target_size(SymbolPresentationRole role) noexcept {
    switch (role) {
    case SymbolPresentationRole::Sidebar:  return 38.0f;
    case SymbolPresentationRole::FileType: return 64.0f;
    case SymbolPresentationRole::Toolbar:
    case SymbolPresentationRole::Navigation:
    case SymbolPresentationRole::Action:
        return 36.0f;
    }
    return 36.0f;
}

inline float activation_hit_target_size(
        SymbolPresentationRole role) noexcept {
    if (role == SymbolPresentationRole::FileType)
        return hit_target_size(role);
    return 44.0f;
}

inline float optical_y_offset(SymbolPresentationRole role) noexcept {
    switch (role) {
    case SymbolPresentationRole::Sidebar:
        return -0.5f;
    case SymbolPresentationRole::Toolbar:
    case SymbolPresentationRole::Navigation:
    case SymbolPresentationRole::FileType:
    case SymbolPresentationRole::Action:
        return 0.0f;
    }
    return 0.0f;
}

inline auto metrics(SymbolPresentationRole role) noexcept -> SymbolMetrics {
    auto const scale = default_scale(role);
    auto const point = point_size(scale);
    auto const target = hit_target_size(role);
    return SymbolMetrics{
        role,
        scale,
        24.0f,
        point,
        target,
        (target - point) * 0.5f,
        1.8f,
        optical_y_offset(role),
    };
}

inline auto symbol_metrics(Symbol symbol) noexcept -> SymbolMetrics {
    auto out = metrics(default_presentation_role(symbol));
    out.stroke_width = descriptor(symbol).default_stroke_width;
    return out;
}

inline auto macos_light_tone_color(SymbolTone tone) noexcept -> SymbolColor {
    switch (tone) {
    case SymbolTone::Primary:     return {30, 30, 30, 255};
    case SymbolTone::Secondary:   return {96, 96, 100, 255};
    case SymbolTone::Selected:    return {58, 58, 60, 255};
    case SymbolTone::Accent:      return {0, 122, 255, 255};
    case SymbolTone::Disabled:    return {190, 193, 198, 255};
    case SymbolTone::Destructive: return {255, 59, 48, 255};
    }
    return {96, 96, 100, 255};
}

inline auto macos_interaction_tone(SymbolPresentationRole role,
                                   SymbolInteractionState state) noexcept
        -> SymbolTone {
    if (!state.enabled)
        return SymbolTone::Disabled;
    if (state.selected) {
        return role == SymbolPresentationRole::Sidebar
            ? SymbolTone::Accent
            : SymbolTone::Selected;
    }
    switch (role) {
    case SymbolPresentationRole::Sidebar:
        return SymbolTone::Primary;
    case SymbolPresentationRole::Toolbar:
    case SymbolPresentationRole::Navigation:
    case SymbolPresentationRole::FileType:
    case SymbolPresentationRole::Action:
        return SymbolTone::Secondary;
    }
    return SymbolTone::Secondary;
}

inline auto macos_interaction_tone(Symbol symbol,
                                   SymbolInteractionState state) noexcept
        -> SymbolTone {
    return macos_interaction_tone(default_presentation_role(symbol), state);
}

inline auto macos_control_chrome(SymbolPresentationRole role,
                                 SymbolInteractionState state) noexcept
        -> SymbolControlChrome {
    auto const tone = macos_interaction_tone(role, state);
    auto const symbol_color = macos_light_tone_color(tone);
    auto const transparent = SymbolColor{0, 0, 0, 0};
    if (!state.enabled) {
        return SymbolControlChrome{
            role,
            tone,
            symbol_color,
            transparent,
            transparent,
            role == SymbolPresentationRole::Sidebar ? 10.0f : 15.0f,
            hit_target_size(role),
            true,
            role != SymbolPresentationRole::FileType,
            symbol_control_chrome_policy(),
        };
    }
    if (role == SymbolPresentationRole::Sidebar) {
        return SymbolControlChrome{
            role,
            tone,
            symbol_color,
            state.selected
                ? SymbolColor{248, 248, 250, 238}
                : transparent,
            state.selected
                ? SymbolColor{242, 242, 246, 248}
                : SymbolColor{230, 230, 234, 150},
            10.0f,
            hit_target_size(role),
            true,
            true,
            symbol_control_chrome_policy(),
        };
    }
    if (role == SymbolPresentationRole::FileType) {
        return SymbolControlChrome{
            role,
            tone,
            symbol_color,
            transparent,
            transparent,
            0.0f,
            hit_target_size(role),
            true,
            false,
            symbol_control_chrome_policy(),
        };
    }
    return SymbolControlChrome{
        role,
        tone,
        symbol_color,
        state.selected
            ? SymbolColor{229, 229, 234, 150}
            : transparent,
        state.selected
            ? SymbolColor{216, 216, 222, 190}
            : SymbolColor{229, 229, 234, 120},
        15.0f,
        hit_target_size(role),
        true,
        true,
        symbol_control_chrome_policy(),
    };
}

inline auto macos_phase_background_color(SymbolPresentationRole role,
                                         SymbolInteractionState state,
                                         SymbolInteractionPhase phase) noexcept
        -> SymbolColor {
    auto const chrome = macos_control_chrome(role, state);
    if (!state.enabled)
        return chrome.background_color;
    switch (phase) {
    case SymbolInteractionPhase::Normal:
        return chrome.background_color;
    case SymbolInteractionPhase::Hovered:
        return chrome.hover_background_color;
    case SymbolInteractionPhase::Pressed:
        if (role == SymbolPresentationRole::FileType)
            return chrome.background_color;
        if (role == SymbolPresentationRole::Sidebar) {
            return state.selected
                ? SymbolColor{232, 232, 238, 255}
                : SymbolColor{218, 218, 224, 190};
        }
        return state.selected
            ? SymbolColor{204, 204, 212, 210}
            : SymbolColor{214, 214, 222, 150};
    }
    return chrome.background_color;
}

inline float macos_phase_symbol_opacity(SymbolInteractionState state,
                                        SymbolInteractionPhase phase) noexcept {
    if (!state.enabled)
        return 0.35f;
    return phase == SymbolInteractionPhase::Pressed ? 0.82f : 1.0f;
}

inline float macos_phase_scale(SymbolPresentationRole role,
                               SymbolInteractionPhase phase) noexcept {
    if (phase != SymbolInteractionPhase::Pressed)
        return 1.0f;
    return role == SymbolPresentationRole::FileType ? 1.0f : 0.985f;
}

inline auto macos_state_recipe(SymbolPresentationRole role,
                               SymbolInteractionState state,
                               SymbolInteractionPhase phase) noexcept
        -> SymbolStateRecipe {
    auto const chrome = macos_control_chrome(role, state);
    return SymbolStateRecipe{
        role,
        phase,
        state.selected,
        state.enabled,
        chrome.symbol_tone,
        chrome.symbol_color,
        macos_phase_background_color(role, state, phase),
        macos_phase_symbol_opacity(state, phase),
        macos_phase_scale(role, phase),
        chrome.corner_radius,
        chrome.hit_target_size,
        chrome.borderless,
        chrome.grouped_control,
        symbol_interaction_phase_policy(),
    };
}

inline auto macos_file_type_color(Symbol symbol) noexcept -> SymbolColor {
    switch (symbol) {
    case Symbol::Folder:
    case Symbol::NewFolder:
        return {64, 156, 255, 255};
    case Symbol::Image:
        return {48, 176, 199, 255};
    case Symbol::Movie:
        return {88, 86, 214, 255};
    case Symbol::AudioDocument:
        return {175, 82, 222, 255};
    case Symbol::CodeDocument:
        return {52, 120, 246, 255};
    case Symbol::SpreadsheetDocument:
        return {52, 199, 89, 255};
    case Symbol::PresentationDocument:
        return {255, 149, 0, 255};
    case Symbol::PdfDocument:
        return {255, 69, 58, 255};
    case Symbol::TextDocument:
        return {90, 128, 168, 255};
    case Symbol::Archive:
        return {196, 142, 64, 255};
    case Symbol::Trash:
        return {142, 142, 147, 255};
    case Symbol::Document:
    case Symbol::NewDocument:
    case Symbol::Duplicate:
        return {142, 142, 147, 255};
    default:
        return macos_light_tone_color(SymbolTone::Secondary);
    }
}

} // namespace phenotype::icon_catalog

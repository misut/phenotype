module;
#include <cstddef>
#include <optional>
#include <string>
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

enum class MaterialSymbolsStyle {
    Outlined,
    Rounded,
    Sharp,
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
    std::string_view style;
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
    std::string_view license_url;
    std::string_view source_acquisition;
    bool used_as_embedded_asset_source = false;
    bool apple_owned_artwork = false;
    bool may_embed_svg_source = false;
    bool requires_notice = false;
    bool runtime_fetch_allowed = false;
    bool platform_extraction_allowed = false;
};

struct FileTypeIconDescriptor {
    Symbol symbol = Symbol::Document;
    std::string_view token;
    std::string_view kind_label;
    std::string package_asset_name;
    std::string package_asset_source;
    std::string_view extension_family;
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
inline constexpr unsigned int phenotype_owned_symbol_count = 0;
inline constexpr unsigned int permissive_source_symbol_count = all_symbol_count;
inline constexpr unsigned int material_symbols_source_symbol_count =
    permissive_source_symbol_count;
inline constexpr unsigned int material_symbols_unique_source_icon_count = 39;
inline constexpr unsigned int material_symbols_style_count = 3;
inline constexpr unsigned int material_symbols_source_variant_count =
    material_symbols_unique_source_icon_count * material_symbols_style_count;
inline constexpr unsigned int apple_asset_symbol_count = 0;
inline constexpr unsigned int platform_extracted_symbol_count = 0;
inline constexpr unsigned int runtime_fetched_symbol_count = 0;
inline constexpr unsigned int audited_symbol_source_count = all_symbol_count;
inline constexpr unsigned int reference_source_count = 6;
inline constexpr unsigned int sidebar_symbol_count = 11;
inline constexpr unsigned int toolbar_symbol_count = 15;
inline constexpr unsigned int file_type_symbol_count = 11;
inline constexpr unsigned int file_type_extension_rule_count = 36;
inline constexpr unsigned int outline_symbol_count = all_symbol_count;
inline constexpr unsigned int filled_symbol_count = all_symbol_count;
inline constexpr unsigned int hierarchical_symbol_count = 0;
inline constexpr unsigned int monochrome_symbol_count = all_symbol_count;
inline constexpr unsigned int regular_weight_symbol_count = all_symbol_count;
inline constexpr unsigned int palette_symbol_count = 0;
inline constexpr unsigned int multicolor_symbol_count = 0;
inline constexpr unsigned int reference_symbol_count = all_symbol_count;
inline constexpr unsigned int svg_path_arc_symbol_count = 0;
inline constexpr unsigned int round_stroke_symbol_count = 0;
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

inline auto material_symbols_style_at(unsigned int index) noexcept
        -> MaterialSymbolsStyle {
    switch (index) {
    case 0: return MaterialSymbolsStyle::Outlined;
    case 1: return MaterialSymbolsStyle::Rounded;
    case 2: return MaterialSymbolsStyle::Sharp;
    }
    return MaterialSymbolsStyle::Outlined;
}

inline auto material_symbols_style_name(MaterialSymbolsStyle style) noexcept
        -> std::string_view {
    switch (style) {
    case MaterialSymbolsStyle::Outlined: return "outlined";
    case MaterialSymbolsStyle::Rounded:  return "rounded";
    case MaterialSymbolsStyle::Sharp:    return "sharp";
    }
    return "outlined";
}

inline auto material_symbols_style_label(MaterialSymbolsStyle style) noexcept
        -> std::string_view {
    switch (style) {
    case MaterialSymbolsStyle::Outlined: return "Outlined";
    case MaterialSymbolsStyle::Rounded:  return "Rounded";
    case MaterialSymbolsStyle::Sharp:    return "Sharp";
    }
    return "Outlined";
}

inline auto material_symbols_font_family(MaterialSymbolsStyle style) noexcept
        -> std::string_view {
    switch (style) {
    case MaterialSymbolsStyle::Outlined: return "Material Symbols Outlined";
    case MaterialSymbolsStyle::Rounded:  return "Material Symbols Rounded";
    case MaterialSymbolsStyle::Sharp:    return "Material Symbols Sharp";
    }
    return "Material Symbols Outlined";
}

inline auto material_symbols_css_class(MaterialSymbolsStyle style) noexcept
        -> std::string_view {
    switch (style) {
    case MaterialSymbolsStyle::Outlined: return "material-symbols-outlined";
    case MaterialSymbolsStyle::Rounded:  return "material-symbols-rounded";
    case MaterialSymbolsStyle::Sharp:    return "material-symbols-sharp";
    }
    return "material-symbols-outlined";
}

inline auto material_symbols_source_directory(
        MaterialSymbolsStyle style) noexcept -> std::string_view {
    switch (style) {
    case MaterialSymbolsStyle::Outlined: return "materialsymbolsoutlined";
    case MaterialSymbolsStyle::Rounded:  return "materialsymbolsrounded";
    case MaterialSymbolsStyle::Sharp:    return "materialsymbolssharp";
    }
    return "materialsymbolsoutlined";
}

inline auto default_material_symbols_style() noexcept
        -> MaterialSymbolsStyle {
    return MaterialSymbolsStyle::Outlined;
}

inline bool is_default_material_symbols_style(
        MaterialSymbolsStyle style) noexcept {
    return style == default_material_symbols_style();
}

inline auto material_symbols_style_from_name(std::string_view name) noexcept
        -> std::optional<MaterialSymbolsStyle> {
    if (name == "outlined" || name == "outline"
        || name == "Outlined" || name == "Outline")
        return MaterialSymbolsStyle::Outlined;
    if (name == "rounded" || name == "round"
        || name == "Rounded" || name == "Round")
        return MaterialSymbolsStyle::Rounded;
    if (name == "sharp" || name == "Sharp")
        return MaterialSymbolsStyle::Sharp;
    return std::nullopt;
}

inline auto material_symbols_style_name_at(unsigned int index) noexcept
        -> std::string_view {
    return material_symbols_style_name(material_symbols_style_at(index));
}

inline auto style_name(MaterialSymbolsStyle style) noexcept -> std::string_view {
    switch (style) {
    case MaterialSymbolsStyle::Outlined:
        return "google_material_symbols_outlined_svg";
    case MaterialSymbolsStyle::Rounded:
        return "google_material_symbols_rounded_svg";
    case MaterialSymbolsStyle::Sharp:
        return "google_material_symbols_sharp_svg";
    }
    return "google_material_symbols_outlined_svg";
}

inline auto style_name() noexcept -> std::string_view {
    return style_name(default_material_symbols_style());
}

inline auto style_reference() noexcept -> std::string_view {
    return "Apple HIG, macOS Finder, and SF Symbols inspired Material Symbols (new) SVG glyphs with Outlined as the default selectable style";
}

inline auto asset_policy() noexcept -> std::string_view {
    return "audited Google Material Symbols (new) vector assets; default Outlined plus selectable Rounded and Sharp styles; no Apple or SF Symbols artwork embedded";
}

inline auto source_license_policy() noexcept -> std::string_view {
    return "audited permissive SVG only; embedded icons use Google Material Symbols (new) Apache-2.0 with bundled license text";
}

inline auto preferred_external_source_policy() noexcept -> std::string_view {
    return "Prefer pinned Google Material Symbols (new) Apache-2.0 SVGs from Google Fonts before phenotype adaptation; Outlined is the default style";
}

inline auto source_acquisition_policy() noexcept -> std::string_view {
    return "development-time import from pinned Google Material Symbols (new) Outlined, Rounded, and Sharp raw SVG URLs only; runtime uses embedded SVG strings, does not fetch remote resources, and keeps platform icon extraction disabled unless redistribution rights are explicit and audited";
}

inline auto document_cache_policy() noexcept -> std::string_view {
    return "explicit_symbol_document_cache_keyed_by_symbol_descriptor_no_frame_parse_churn";
}

inline auto source_attribution_policy() noexcept -> std::string_view {
    return "embedded Google Material Symbols SVG sources must expose family, icon name, style, exact license, pinned direct raw SVG URL, source revision, copyright, Apple-asset boundary, platform extraction flag, and runtime fetch flag in debug output";
}

inline auto apple_asset_boundary() noexcept -> std::string_view {
    return "Apple SF Symbols, Finder icons, and macOS system icons are design references only; do not extract or embed Apple-owned artwork unless explicit redistribution clearance is recorded";
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
            "https://www.apple.com/legal/intellectual-property/guidelinesfor3rdparties.html",
            "reference_only_no_embedding",
            false,
            true,
            false,
            false,
            false,
            false,
        };
    case 1:
        return {
            "Apple Human Interface Guidelines: SF Symbols",
            "https://developer.apple.com/design/human-interface-guidelines/sf-symbols",
            "semantic symbol naming and rendering mode reference",
            "reference only; do not embed SF Symbols artwork",
            "https://developer.apple.com/design/human-interface-guidelines/sf-symbols",
            "semantic_reference_only_no_exported_vectors",
            false,
            true,
            false,
            false,
            false,
            false,
        };
    case 2:
        return {
            "W3C SVG 2 Paths",
            "https://www.w3.org/TR/SVG2/paths.html",
            "SVG path command parser reference",
            "open web standard reference",
            "https://www.w3.org/copyright/software-license/",
            "parser_reference_only",
            false,
            false,
            false,
            false,
            false,
            false,
        };
    case 3:
        return {
            "Google Material Symbols",
            "https://developers.google.com/fonts/docs/material_symbols",
            "audited permissive SVG source for embedded glyphs",
            "Apache-2.0 license; bundled license text when embedded",
            "https://github.com/google/material-design-icons/blob/4d7678801370dc2fe9c35b437570f56f56e43801/LICENSE",
            "pinned_raw_svg_development_time_import",
            true,
            false,
            true,
            true,
            false,
            false,
        };
    case 4:
        return {
            "Tabler Icons",
            "https://github.com/tabler/tabler-icons",
            "future permissive fallback source candidate",
            "MIT license with attribution when embedded",
            "https://github.com/tabler/tabler-icons/blob/main/LICENSE",
            "candidate_pinned_raw_svg_development_time_import",
            false,
            false,
            true,
            true,
            false,
            false,
        };
    case 5:
        return {
            "Iconoir",
            "https://github.com/iconoir-icons/iconoir",
            "future permissive fallback source candidate",
            "MIT license with attribution when embedded",
            "https://github.com/iconoir-icons/iconoir/blob/main/LICENSE",
            "candidate_pinned_raw_svg_development_time_import",
            false,
            false,
            true,
            true,
            false,
            false,
        };
    }
    return reference_source_at(0);
}

inline auto reference_family() noexcept -> std::string_view {
    return "SF Symbols semantic reference with Google Material Symbols artwork";
}

inline auto reference_policy() noexcept -> std::string_view {
    return "semantic reference only; no Apple or SF Symbols vector artwork; embedded sources are Google Material Symbols (new) SVG";
}

inline auto presentation_policy() noexcept -> std::string_view {
    return "macos_role_aware_symbol_presentation";
}

inline auto source_format() noexcept -> std::string_view {
    return "svg";
}

inline auto svg_subset_policy() noexcept -> std::string_view {
    return "bounded_material_symbols_svg_subset";
}

inline auto svg_supported_elements() noexcept -> std::string_view {
    return "svg, g, path, rect, circle, ellipse, line, polyline, polygon";
}

inline auto svg_supported_path_commands() noexcept -> std::string_view {
    return "M L H V Q T C S Z with absolute and relative variants";
}

inline auto svg_supported_style_attributes() noexcept -> std::string_view {
    return "fill, stroke, color, opacity, fill-opacity, stroke-opacity, stroke-width, stroke-linecap, stroke-linejoin";
}

inline auto svg_arc_policy() noexcept -> std::string_view {
    return "Google Material Symbols source paths avoid SVG arc commands in the pinned subset";
}

inline auto stroke_geometry_policy() noexcept -> std::string_view {
    return "material_symbols_filled_path_geometry";
}

inline auto stroke_cap_policy() noexcept -> std::string_view {
    return "not_applicable";
}

inline auto stroke_join_policy() noexcept -> std::string_view {
    return "not_applicable";
}

inline auto alignment_policy() noexcept -> std::string_view {
    return "24x24 text-aligned symbol grid";
}

inline auto variant_policy() noexcept -> std::string_view {
    return "Material Symbols Outlined is the default; Rounded and Sharp are selectable with role-aware symbol chrome";
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
    return "consistent_material_symbols_new_style_selectable_size_detail_and_perspective";
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
    return "regular_material_symbols_weight_aligned";
}

inline auto rendering_capability_policy() noexcept -> std::string_view {
    return "material_symbols_monochrome_filled_path_contract";
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

inline char ascii_lower(char ch) noexcept {
    return ch >= 'A' && ch <= 'Z'
        ? static_cast<char>(ch - 'A' + 'a')
        : ch;
}

inline bool ascii_equal_fold(std::string_view lhs,
                             std::string_view rhs) noexcept {
    if (lhs.size() != rhs.size())
        return false;
    for (std::size_t i = 0; i < lhs.size(); ++i) {
        if (ascii_lower(lhs[i]) != ascii_lower(rhs[i]))
            return false;
    }
    return true;
}

inline auto file_type_token(Symbol symbol) noexcept -> std::string_view {
    switch (symbol) {
    case Symbol::Folder:               return "folder";
    case Symbol::Document:             return "document";
    case Symbol::PdfDocument:          return "pdf";
    case Symbol::TextDocument:         return "text";
    case Symbol::Image:                return "image";
    case Symbol::Movie:                return "movie";
    case Symbol::Archive:              return "archive";
    case Symbol::AudioDocument:        return "audio";
    case Symbol::CodeDocument:         return "code";
    case Symbol::SpreadsheetDocument:  return "spreadsheet";
    case Symbol::PresentationDocument: return "presentation";
    default:                           return "document";
    }
}

inline auto file_type_kind_label(Symbol symbol) noexcept -> std::string_view {
    switch (symbol) {
    case Symbol::Folder:               return "Folder";
    case Symbol::PdfDocument:          return "PDF Document";
    case Symbol::TextDocument:         return "Text Document";
    case Symbol::Image:                return "Image";
    case Symbol::Movie:                return "Movie";
    case Symbol::Archive:              return "Archive";
    case Symbol::AudioDocument:        return "Audio";
    case Symbol::CodeDocument:         return "Code";
    case Symbol::SpreadsheetDocument:  return "Spreadsheet";
    case Symbol::PresentationDocument: return "Presentation";
    case Symbol::Document:
    default:                           return "Document";
    }
}

inline auto file_type_extension_family(Symbol symbol) noexcept
        -> std::string_view {
    switch (symbol) {
    case Symbol::Folder:               return "directory";
    case Symbol::PdfDocument:          return "pdf";
    case Symbol::TextDocument:         return "plain_text";
    case Symbol::Image:                return "raster_or_svg_image";
    case Symbol::Movie:                return "movie";
    case Symbol::Archive:              return "archive";
    case Symbol::AudioDocument:        return "audio";
    case Symbol::CodeDocument:         return "source_code";
    case Symbol::SpreadsheetDocument:  return "spreadsheet";
    case Symbol::PresentationDocument: return "presentation";
    case Symbol::Document:
    default:                           return "generic_document";
    }
}

inline auto file_type_symbol_from_token(std::string_view token) noexcept
        -> std::optional<Symbol> {
    for (unsigned int i = 0; i < file_type_symbol_count; ++i) {
        auto const symbol = file_type_symbol_at(i);
        if (file_type_token(symbol) == token)
            return symbol;
    }
    return std::nullopt;
}

inline bool is_file_type_symbol(Symbol symbol) noexcept {
    for (unsigned int i = 0; i < file_type_symbol_count; ++i) {
        if (file_type_symbol_at(i) == symbol)
            return true;
    }
    return false;
}

inline auto file_type_package_asset_name(Symbol symbol) -> std::string {
    auto out = std::string{"file_type."};
    out += file_type_token(symbol);
    out += ".icon";
    return out;
}

inline auto file_type_package_asset_source(Symbol symbol) -> std::string {
    auto out = std::string{"assets/icons/file-types/"};
    out += file_type_token(symbol);
    out += ".svg";
    return out;
}

inline auto file_type_icon_descriptor(Symbol symbol)
        -> FileTypeIconDescriptor {
    auto const normalized =
        is_file_type_symbol(symbol) ? symbol : Symbol::Document;
    return FileTypeIconDescriptor{
        normalized,
        file_type_token(normalized),
        file_type_kind_label(normalized),
        file_type_package_asset_name(normalized),
        file_type_package_asset_source(normalized),
        file_type_extension_family(normalized),
    };
}

inline bool file_extension_is_svg_image(std::string_view ext) noexcept {
    return ascii_equal_fold(ext, "svg") || ascii_equal_fold(ext, "svgz");
}

inline bool file_extension_is_raster_image(std::string_view ext) noexcept {
    return ascii_equal_fold(ext, "png")
        || ascii_equal_fold(ext, "jpg")
        || ascii_equal_fold(ext, "jpeg")
        || ascii_equal_fold(ext, "heic")
        || ascii_equal_fold(ext, "heif")
        || ascii_equal_fold(ext, "webp");
}

inline bool file_extension_is_image(std::string_view ext) noexcept {
    return file_extension_is_raster_image(ext)
        || file_extension_is_svg_image(ext);
}

inline bool file_extension_is_movie(std::string_view ext) noexcept {
    return ascii_equal_fold(ext, "mov")
        || ascii_equal_fold(ext, "mp4")
        || ascii_equal_fold(ext, "m4v");
}

inline bool file_extension_is_audio(std::string_view ext) noexcept {
    return ascii_equal_fold(ext, "mp3")
        || ascii_equal_fold(ext, "m4a")
        || ascii_equal_fold(ext, "wav")
        || ascii_equal_fold(ext, "aac")
        || ascii_equal_fold(ext, "flac");
}

inline bool file_extension_is_code(std::string_view ext) noexcept {
    return ascii_equal_fold(ext, "cpp")
        || ascii_equal_fold(ext, "cc")
        || ascii_equal_fold(ext, "cxx")
        || ascii_equal_fold(ext, "h")
        || ascii_equal_fold(ext, "hpp")
        || ascii_equal_fold(ext, "js")
        || ascii_equal_fold(ext, "ts")
        || ascii_equal_fold(ext, "json")
        || ascii_equal_fold(ext, "toml");
}

inline bool file_extension_is_spreadsheet(std::string_view ext) noexcept {
    return ascii_equal_fold(ext, "csv")
        || ascii_equal_fold(ext, "xls")
        || ascii_equal_fold(ext, "xlsx")
        || ascii_equal_fold(ext, "numbers");
}

inline bool file_extension_is_presentation(std::string_view ext) noexcept {
    return ascii_equal_fold(ext, "key")
        || ascii_equal_fold(ext, "ppt")
        || ascii_equal_fold(ext, "pptx");
}

inline bool file_extension_is_archive(std::string_view ext) noexcept {
    return ascii_equal_fold(ext, "zip");
}

inline auto known_file_type_symbol_for_extension(
        std::string_view ext) noexcept -> std::optional<Symbol> {
    if (ascii_equal_fold(ext, "pdf"))
        return Symbol::PdfDocument;
    if (file_extension_is_image(ext))
        return Symbol::Image;
    if (file_extension_is_movie(ext))
        return Symbol::Movie;
    if (ascii_equal_fold(ext, "txt") || ascii_equal_fold(ext, "md"))
        return Symbol::TextDocument;
    if (file_extension_is_archive(ext))
        return Symbol::Archive;
    if (file_extension_is_audio(ext))
        return Symbol::AudioDocument;
    if (file_extension_is_code(ext))
        return Symbol::CodeDocument;
    if (file_extension_is_spreadsheet(ext))
        return Symbol::SpreadsheetDocument;
    if (file_extension_is_presentation(ext))
        return Symbol::PresentationDocument;
    return std::nullopt;
}

inline auto known_file_type_kind_label_for_extension(
        std::string_view ext) noexcept -> std::optional<std::string_view> {
    if (file_extension_is_svg_image(ext))
        return std::string_view{"SVG Image"};
    auto const symbol = known_file_type_symbol_for_extension(ext);
    if (!symbol.has_value())
        return std::nullopt;
    return file_type_kind_label(*symbol);
}

inline auto file_type_symbol_for_extension(std::string_view ext) noexcept
        -> Symbol {
    auto const known = known_file_type_symbol_for_extension(ext);
    return known.has_value() ? *known : Symbol::Document;
}

inline auto file_type_symbol_for_entry(std::string_view ext,
                                       bool directory) noexcept -> Symbol {
    return directory ? Symbol::Folder : file_type_symbol_for_extension(ext);
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

inline bool uses_material_symbols_source(Symbol symbol) noexcept {
    switch (symbol) {
    case Symbol::Back:
    case Symbol::Forward:
    case Symbol::Search:
    case Symbol::Share:
    case Symbol::Tag:
    case Symbol::More:
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
    case Symbol::AirDrop:
    case Symbol::Recents:
    case Symbol::Shared:
    case Symbol::Sidebar:
    case Symbol::NewFolder:
    case Symbol::Applications:
    case Symbol::Desktop:
    case Symbol::Download:
    case Symbol::SortGroup:
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

inline auto material_symbols_source_icon_name(Symbol symbol) noexcept
        -> std::string_view {
    switch (symbol) {
    case Symbol::Back: return "chevron_left";
    case Symbol::Forward: return "chevron_right";
    case Symbol::Search: return "search";
    case Symbol::Share: return "ios_share";
    case Symbol::Tag: return "sell";
    case Symbol::More: return "more_horiz";
    case Symbol::Grid: return "grid_view";
    case Symbol::List: return "view_list";
    case Symbol::Columns: return "view_column";
    case Symbol::Gallery: return "view_carousel";
    case Symbol::Folder: return "folder";
    case Symbol::Trash: return "delete";
    case Symbol::Document: return "draft";
    case Symbol::Image: return "image";
    case Symbol::Movie: return "movie";
    case Symbol::Plus: return "add";
    case Symbol::XMark: return "close";
    case Symbol::ChevronDown: return "keyboard_arrow_down";
    case Symbol::ChevronUp: return "keyboard_arrow_up";
    case Symbol::Home: return "home";
    case Symbol::Cloud: return "cloud";
    case Symbol::AirDrop: return "nearby";
    case Symbol::Recents: return "history";
    case Symbol::Shared: return "folder_shared";
    case Symbol::Sidebar: return "side_navigation";
    case Symbol::NewFolder: return "create_new_folder";
    case Symbol::Applications: return "apps";
    case Symbol::Desktop: return "desktop_mac";
    case Symbol::Download: return "download";
    case Symbol::SortGroup: return "swap_vert";
    case Symbol::Duplicate: return "content_copy";
    case Symbol::NewDocument: return "note_add";
    case Symbol::PdfDocument: return "picture_as_pdf";
    case Symbol::TextDocument: return "article";
    case Symbol::Archive: return "folder_zip";
    case Symbol::AudioDocument: return "audio_file";
    case Symbol::CodeDocument: return "code";
    case Symbol::SpreadsheetDocument: return "table_chart";
    case Symbol::PresentationDocument: return "slideshow";
    default: return name(symbol);
    }
}

inline auto permissive_source_icon_name(Symbol symbol) noexcept
        -> std::string_view {
    return material_symbols_source_icon_name(symbol);
}

inline auto material_symbols_source_icon_name_at(unsigned int index) noexcept
        -> std::string_view {
    switch (index) {
    case 0: return "chevron_left";
    case 1: return "chevron_right";
    case 2: return "search";
    case 3: return "ios_share";
    case 4: return "sell";
    case 5: return "more_horiz";
    case 6: return "grid_view";
    case 7: return "view_list";
    case 8: return "view_column";
    case 9: return "view_carousel";
    case 10: return "folder";
    case 11: return "delete";
    case 12: return "draft";
    case 13: return "image";
    case 14: return "movie";
    case 15: return "add";
    case 16: return "close";
    case 17: return "keyboard_arrow_down";
    case 18: return "keyboard_arrow_up";
    case 19: return "home";
    case 20: return "cloud";
    case 21: return "nearby";
    case 22: return "history";
    case 23: return "folder_shared";
    case 24: return "side_navigation";
    case 25: return "create_new_folder";
    case 26: return "apps";
    case 27: return "desktop_mac";
    case 28: return "download";
    case 29: return "swap_vert";
    case 30: return "content_copy";
    case 31: return "note_add";
    case 32: return "picture_as_pdf";
    case 33: return "article";
    case 34: return "folder_zip";
    case 35: return "audio_file";
    case 36: return "code";
    case 37: return "table_chart";
    case 38: return "slideshow";
    }
    return "draft";
}

inline auto material_symbols_source_revision() noexcept -> std::string_view {
    return "4d7678801370dc2fe9c35b437570f56f56e43801";
}

inline auto material_symbols_source_url(std::string_view icon_name,
                                        MaterialSymbolsStyle style) noexcept
        -> std::string_view {
    switch (style) {
    case MaterialSymbolsStyle::Outlined:
        if (icon_name == "chevron_left")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/chevron_left/materialsymbolsoutlined/chevron_left_24px.svg";
        if (icon_name == "chevron_right")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/chevron_right/materialsymbolsoutlined/chevron_right_24px.svg";
        if (icon_name == "search")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/search/materialsymbolsoutlined/search_24px.svg";
        if (icon_name == "ios_share")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/ios_share/materialsymbolsoutlined/ios_share_24px.svg";
        if (icon_name == "sell")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/sell/materialsymbolsoutlined/sell_24px.svg";
        if (icon_name == "more_horiz")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/more_horiz/materialsymbolsoutlined/more_horiz_24px.svg";
        if (icon_name == "grid_view")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/grid_view/materialsymbolsoutlined/grid_view_24px.svg";
        if (icon_name == "view_list")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_list/materialsymbolsoutlined/view_list_24px.svg";
        if (icon_name == "view_column")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_column/materialsymbolsoutlined/view_column_24px.svg";
        if (icon_name == "view_carousel")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_carousel/materialsymbolsoutlined/view_carousel_24px.svg";
        if (icon_name == "folder")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder/materialsymbolsoutlined/folder_24px.svg";
        if (icon_name == "delete")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/delete/materialsymbolsoutlined/delete_24px.svg";
        if (icon_name == "draft")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/draft/materialsymbolsoutlined/draft_24px.svg";
        if (icon_name == "image")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/image/materialsymbolsoutlined/image_24px.svg";
        if (icon_name == "movie")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/movie/materialsymbolsoutlined/movie_24px.svg";
        if (icon_name == "add")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/add/materialsymbolsoutlined/add_24px.svg";
        if (icon_name == "close")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/close/materialsymbolsoutlined/close_24px.svg";
        if (icon_name == "keyboard_arrow_down")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/keyboard_arrow_down/materialsymbolsoutlined/keyboard_arrow_down_24px.svg";
        if (icon_name == "keyboard_arrow_up")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/keyboard_arrow_up/materialsymbolsoutlined/keyboard_arrow_up_24px.svg";
        if (icon_name == "home")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/home/materialsymbolsoutlined/home_24px.svg";
        if (icon_name == "cloud")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/cloud/materialsymbolsoutlined/cloud_24px.svg";
        if (icon_name == "nearby")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/nearby/materialsymbolsoutlined/nearby_24px.svg";
        if (icon_name == "history")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/history/materialsymbolsoutlined/history_24px.svg";
        if (icon_name == "folder_shared")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder_shared/materialsymbolsoutlined/folder_shared_24px.svg";
        if (icon_name == "side_navigation")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/side_navigation/materialsymbolsoutlined/side_navigation_24px.svg";
        if (icon_name == "create_new_folder")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/create_new_folder/materialsymbolsoutlined/create_new_folder_24px.svg";
        if (icon_name == "apps")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/apps/materialsymbolsoutlined/apps_24px.svg";
        if (icon_name == "desktop_mac")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/desktop_mac/materialsymbolsoutlined/desktop_mac_24px.svg";
        if (icon_name == "download")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/download/materialsymbolsoutlined/download_24px.svg";
        if (icon_name == "swap_vert")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/swap_vert/materialsymbolsoutlined/swap_vert_24px.svg";
        if (icon_name == "content_copy")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/content_copy/materialsymbolsoutlined/content_copy_24px.svg";
        if (icon_name == "note_add")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/note_add/materialsymbolsoutlined/note_add_24px.svg";
        if (icon_name == "picture_as_pdf")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/picture_as_pdf/materialsymbolsoutlined/picture_as_pdf_24px.svg";
        if (icon_name == "article")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/article/materialsymbolsoutlined/article_24px.svg";
        if (icon_name == "folder_zip")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder_zip/materialsymbolsoutlined/folder_zip_24px.svg";
        if (icon_name == "audio_file")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/audio_file/materialsymbolsoutlined/audio_file_24px.svg";
        if (icon_name == "code")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/code/materialsymbolsoutlined/code_24px.svg";
        if (icon_name == "table_chart")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/table_chart/materialsymbolsoutlined/table_chart_24px.svg";
        if (icon_name == "slideshow")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/slideshow/materialsymbolsoutlined/slideshow_24px.svg";
        break;
    case MaterialSymbolsStyle::Rounded:
        if (icon_name == "chevron_left")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/chevron_left/materialsymbolsrounded/chevron_left_24px.svg";
        if (icon_name == "chevron_right")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/chevron_right/materialsymbolsrounded/chevron_right_24px.svg";
        if (icon_name == "search")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/search/materialsymbolsrounded/search_24px.svg";
        if (icon_name == "ios_share")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/ios_share/materialsymbolsrounded/ios_share_24px.svg";
        if (icon_name == "sell")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/sell/materialsymbolsrounded/sell_24px.svg";
        if (icon_name == "more_horiz")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/more_horiz/materialsymbolsrounded/more_horiz_24px.svg";
        if (icon_name == "grid_view")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/grid_view/materialsymbolsrounded/grid_view_24px.svg";
        if (icon_name == "view_list")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_list/materialsymbolsrounded/view_list_24px.svg";
        if (icon_name == "view_column")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_column/materialsymbolsrounded/view_column_24px.svg";
        if (icon_name == "view_carousel")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_carousel/materialsymbolsrounded/view_carousel_24px.svg";
        if (icon_name == "folder")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder/materialsymbolsrounded/folder_24px.svg";
        if (icon_name == "delete")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/delete/materialsymbolsrounded/delete_24px.svg";
        if (icon_name == "draft")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/draft/materialsymbolsrounded/draft_24px.svg";
        if (icon_name == "image")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/image/materialsymbolsrounded/image_24px.svg";
        if (icon_name == "movie")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/movie/materialsymbolsrounded/movie_24px.svg";
        if (icon_name == "add")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/add/materialsymbolsrounded/add_24px.svg";
        if (icon_name == "close")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/close/materialsymbolsrounded/close_24px.svg";
        if (icon_name == "keyboard_arrow_down")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/keyboard_arrow_down/materialsymbolsrounded/keyboard_arrow_down_24px.svg";
        if (icon_name == "keyboard_arrow_up")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/keyboard_arrow_up/materialsymbolsrounded/keyboard_arrow_up_24px.svg";
        if (icon_name == "home")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/home/materialsymbolsrounded/home_24px.svg";
        if (icon_name == "cloud")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/cloud/materialsymbolsrounded/cloud_24px.svg";
        if (icon_name == "nearby")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/nearby/materialsymbolsrounded/nearby_24px.svg";
        if (icon_name == "history")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/history/materialsymbolsrounded/history_24px.svg";
        if (icon_name == "folder_shared")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder_shared/materialsymbolsrounded/folder_shared_24px.svg";
        if (icon_name == "side_navigation")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/side_navigation/materialsymbolsrounded/side_navigation_24px.svg";
        if (icon_name == "create_new_folder")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/create_new_folder/materialsymbolsrounded/create_new_folder_24px.svg";
        if (icon_name == "apps")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/apps/materialsymbolsrounded/apps_24px.svg";
        if (icon_name == "desktop_mac")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/desktop_mac/materialsymbolsrounded/desktop_mac_24px.svg";
        if (icon_name == "download")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/download/materialsymbolsrounded/download_24px.svg";
        if (icon_name == "swap_vert")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/swap_vert/materialsymbolsrounded/swap_vert_24px.svg";
        if (icon_name == "content_copy")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/content_copy/materialsymbolsrounded/content_copy_24px.svg";
        if (icon_name == "note_add")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/note_add/materialsymbolsrounded/note_add_24px.svg";
        if (icon_name == "picture_as_pdf")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/picture_as_pdf/materialsymbolsrounded/picture_as_pdf_24px.svg";
        if (icon_name == "article")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/article/materialsymbolsrounded/article_24px.svg";
        if (icon_name == "folder_zip")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder_zip/materialsymbolsrounded/folder_zip_24px.svg";
        if (icon_name == "audio_file")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/audio_file/materialsymbolsrounded/audio_file_24px.svg";
        if (icon_name == "code")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/code/materialsymbolsrounded/code_24px.svg";
        if (icon_name == "table_chart")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/table_chart/materialsymbolsrounded/table_chart_24px.svg";
        if (icon_name == "slideshow")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/slideshow/materialsymbolsrounded/slideshow_24px.svg";
        break;
    case MaterialSymbolsStyle::Sharp:
        if (icon_name == "chevron_left")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/chevron_left/materialsymbolssharp/chevron_left_24px.svg";
        if (icon_name == "chevron_right")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/chevron_right/materialsymbolssharp/chevron_right_24px.svg";
        if (icon_name == "search")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/search/materialsymbolssharp/search_24px.svg";
        if (icon_name == "ios_share")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/ios_share/materialsymbolssharp/ios_share_24px.svg";
        if (icon_name == "sell")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/sell/materialsymbolssharp/sell_24px.svg";
        if (icon_name == "more_horiz")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/more_horiz/materialsymbolssharp/more_horiz_24px.svg";
        if (icon_name == "grid_view")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/grid_view/materialsymbolssharp/grid_view_24px.svg";
        if (icon_name == "view_list")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_list/materialsymbolssharp/view_list_24px.svg";
        if (icon_name == "view_column")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_column/materialsymbolssharp/view_column_24px.svg";
        if (icon_name == "view_carousel")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/view_carousel/materialsymbolssharp/view_carousel_24px.svg";
        if (icon_name == "folder")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder/materialsymbolssharp/folder_24px.svg";
        if (icon_name == "delete")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/delete/materialsymbolssharp/delete_24px.svg";
        if (icon_name == "draft")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/draft/materialsymbolssharp/draft_24px.svg";
        if (icon_name == "image")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/image/materialsymbolssharp/image_24px.svg";
        if (icon_name == "movie")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/movie/materialsymbolssharp/movie_24px.svg";
        if (icon_name == "add")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/add/materialsymbolssharp/add_24px.svg";
        if (icon_name == "close")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/close/materialsymbolssharp/close_24px.svg";
        if (icon_name == "keyboard_arrow_down")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/keyboard_arrow_down/materialsymbolssharp/keyboard_arrow_down_24px.svg";
        if (icon_name == "keyboard_arrow_up")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/keyboard_arrow_up/materialsymbolssharp/keyboard_arrow_up_24px.svg";
        if (icon_name == "home")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/home/materialsymbolssharp/home_24px.svg";
        if (icon_name == "cloud")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/cloud/materialsymbolssharp/cloud_24px.svg";
        if (icon_name == "nearby")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/nearby/materialsymbolssharp/nearby_24px.svg";
        if (icon_name == "history")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/history/materialsymbolssharp/history_24px.svg";
        if (icon_name == "folder_shared")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder_shared/materialsymbolssharp/folder_shared_24px.svg";
        if (icon_name == "side_navigation")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/side_navigation/materialsymbolssharp/side_navigation_24px.svg";
        if (icon_name == "create_new_folder")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/create_new_folder/materialsymbolssharp/create_new_folder_24px.svg";
        if (icon_name == "apps")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/apps/materialsymbolssharp/apps_24px.svg";
        if (icon_name == "desktop_mac")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/desktop_mac/materialsymbolssharp/desktop_mac_24px.svg";
        if (icon_name == "download")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/download/materialsymbolssharp/download_24px.svg";
        if (icon_name == "swap_vert")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/swap_vert/materialsymbolssharp/swap_vert_24px.svg";
        if (icon_name == "content_copy")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/content_copy/materialsymbolssharp/content_copy_24px.svg";
        if (icon_name == "note_add")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/note_add/materialsymbolssharp/note_add_24px.svg";
        if (icon_name == "picture_as_pdf")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/picture_as_pdf/materialsymbolssharp/picture_as_pdf_24px.svg";
        if (icon_name == "article")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/article/materialsymbolssharp/article_24px.svg";
        if (icon_name == "folder_zip")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/folder_zip/materialsymbolssharp/folder_zip_24px.svg";
        if (icon_name == "audio_file")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/audio_file/materialsymbolssharp/audio_file_24px.svg";
        if (icon_name == "code")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/code/materialsymbolssharp/code_24px.svg";
        if (icon_name == "table_chart")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/table_chart/materialsymbolssharp/table_chart_24px.svg";
        if (icon_name == "slideshow")
            return "https://raw.githubusercontent.com/google/material-design-icons/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web/slideshow/materialsymbolssharp/slideshow_24px.svg";
        break;
    }
    return "https://github.com/google/material-design-icons/tree/4d7678801370dc2fe9c35b437570f56f56e43801/symbols/web";
}

inline auto material_symbols_source_url(std::string_view icon_name) noexcept
        -> std::string_view {
    return material_symbols_source_url(icon_name, default_material_symbols_style());
}

inline auto material_symbols_license_url() noexcept -> std::string_view {
    return "https://github.com/google/material-design-icons/blob/4d7678801370dc2fe9c35b437570f56f56e43801/LICENSE";
}

inline auto material_symbols_icon_license(std::string_view) noexcept
        -> std::string_view {
    return "Apache-2.0";
}

inline auto material_symbols_icon_copyright(std::string_view) noexcept
        -> std::string_view {
    return "Material Symbols by Google; see Apache-2.0 license";
}

inline auto source_attribution(Symbol symbol,
                               MaterialSymbolsStyle style) noexcept
        -> SymbolSourceAttribution {
    if (uses_material_symbols_source(symbol)) {
        auto const icon = material_symbols_source_icon_name(symbol);
        return SymbolSourceAttribution{
            "Google Material Symbols",
            icon,
            material_symbols_style_name(style),
            material_symbols_icon_license(icon),
            material_symbols_license_url(),
            material_symbols_source_url(icon, style),
            material_symbols_source_revision(),
            material_symbols_icon_copyright(icon),
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
        "default",
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

inline auto source_attribution(Symbol symbol) noexcept
        -> SymbolSourceAttribution {
    return source_attribution(symbol, default_material_symbols_style());
}

inline auto material_symbols_svg_source(Symbol symbol,
                                        MaterialSymbolsStyle style) noexcept
        -> std::string_view {
    switch (style) {
    case MaterialSymbolsStyle::Outlined:
        switch (symbol) {
        case Symbol::Back:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M560-240 320-480l240-240 56 56-184 184 184 184-56 56Z"/></svg>)SVG";
        case Symbol::Forward:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M504-480 320-664l56-56 240 240-240 240-56-56 184-184Z"/></svg>)SVG";
        case Symbol::Search:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M784-120 532-372q-30 24-69 38t-83 14q-109 0-184.5-75.5T120-580q0-109 75.5-184.5T380-840q109 0 184.5 75.5T640-580q0 44-14 83t-38 69l252 252-56 56ZM380-400q75 0 127.5-52.5T560-580q0-75-52.5-127.5T380-760q-75 0-127.5 52.5T200-580q0 75 52.5 127.5T380-400Z"/></svg>)SVG";
        case Symbol::Share:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-40q-33 0-56.5-23.5T160-120v-440q0-33 23.5-56.5T240-640h120v80H240v440h480v-440H600v-80h120q33 0 56.5 23.5T800-560v440q0 33-23.5 56.5T720-40H240Zm200-280v-447l-64 64-56-57 160-160 160 160-56 57-64-64v447h-80Z"/></svg>)SVG";
        case Symbol::Tag:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M856-390 570-104q-12 12-27 18t-30 6q-15 0-30-6t-27-18L103-457q-11-11-17-25.5T80-513v-287q0-33 23.5-56.5T160-880h287q16 0 31 6.5t26 17.5l352 353q12 12 17.5 27t5.5 30q0 15-5.5 29.5T856-390ZM513-160l286-286-353-354H160v286l353 354ZM260-640q25 0 42.5-17.5T320-700q0-25-17.5-42.5T260-760q-25 0-42.5 17.5T200-700q0 25 17.5 42.5T260-640Zm220 160Z"/></svg>)SVG";
        case Symbol::More:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-400q-33 0-56.5-23.5T160-480q0-33 23.5-56.5T240-560q33 0 56.5 23.5T320-480q0 33-23.5 56.5T240-400Zm240 0q-33 0-56.5-23.5T400-480q0-33 23.5-56.5T480-560q33 0 56.5 23.5T560-480q0 33-23.5 56.5T480-400Zm240 0q-33 0-56.5-23.5T640-480q0-33 23.5-56.5T720-560q33 0 56.5 23.5T800-480q0 33-23.5 56.5T720-400Z"/></svg>)SVG";
        case Symbol::Grid:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M120-520v-320h320v320H120Zm0 400v-320h320v320H120Zm400-400v-320h320v320H520Zm0 400v-320h320v320H520ZM200-600h160v-160H200v160Zm400 0h160v-160H600v160Zm0 400h160v-160H600v160Zm-400 0h160v-160H200v160Zm400-400Zm0 240Zm-240 0Zm0-240Z"/></svg>)SVG";
        case Symbol::List:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M360-240h440v-107H360v107ZM160-613h120v-107H160v107Zm0 187h120v-107H160v107Zm0 186h120v-107H160v107Zm200-186h440v-107H360v107Zm0-187h440v-107H360v107ZM160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h640q33 0 56.5 23.5T880-720v480q0 33-23.5 56.5T800-160H160Z"/></svg>)SVG";
        case Symbol::Columns:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M121-280v-400q0-33 23.5-56.5T201-760h559q33 0 56.5 23.5T840-680v400q0 33-23.5 56.5T760-200H201q-33 0-56.5-23.5T121-280Zm79 0h133v-400H200v400Zm213 0h133v-400H413v400Zm213 0h133v-400H626v400Z"/></svg>)SVG";
        case Symbol::Gallery:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M80-360v-240q0-33 23.5-56.5T160-680q33 0 56.5 23.5T240-600v240q0 33-23.5 56.5T160-280q-33 0-56.5-23.5T80-360Zm280 160q-33 0-56.5-23.5T280-280v-400q0-33 23.5-56.5T360-760h240q33 0 56.5 23.5T680-680v400q0 33-23.5 56.5T600-200H360Zm360-160v-240q0-33 23.5-56.5T800-680q33 0 56.5 23.5T880-600v240q0 33-23.5 56.5T800-280q-33 0-56.5-23.5T720-360Zm-360 80h240v-400H360v400Zm120-200Z"/></svg>)SVG";
        case Symbol::Folder:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h240l80 80h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160Zm0-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Z"/></svg>)SVG";
        case Symbol::Trash:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M280-120q-33 0-56.5-23.5T200-200v-520h-40v-80h200v-40h240v40h200v80h-40v520q0 33-23.5 56.5T680-120H280Zm400-600H280v520h400v-520ZM360-280h80v-360h-80v360Zm160 0h80v-360h-80v360ZM280-720v520-520Z"/></svg>)SVG";
        case Symbol::Document:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-80q-33 0-56.5-23.5T160-160v-640q0-33 23.5-56.5T240-880h320l240 240v480q0 33-23.5 56.5T720-80H240Zm280-520v-200H240v640h480v-440H520ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::Image:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm0-80h560v-560H200v560Zm40-80h480L570-480 450-320l-90-120-120 160Zm-40 80v-560 560Z"/></svg>)SVG";
        case Symbol::Movie:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="m160-800 80 160h120l-80-160h80l80 160h120l-80-160h80l80 160h120l-80-160h120q33 0 56.5 23.5T880-720v480q0 33-23.5 56.5T800-160H160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800Zm0 240v320h640v-320H160Zm0 0v320-320Z"/></svg>)SVG";
        case Symbol::Plus:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M440-440H200v-80h240v-240h80v240h240v80H520v240h-80v-240Z"/></svg>)SVG";
        case Symbol::XMark:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="m256-200-56-56 224-224-224-224 56-56 224 224 224-224 56 56-224 224 224 224-56 56-224-224-224 224Z"/></svg>)SVG";
        case Symbol::ChevronDown:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-344 240-584l56-56 184 184 184-184 56 56-240 240Z"/></svg>)SVG";
        case Symbol::ChevronUp:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-528 296-344l-56-56 240-240 240 240-56 56-184-184Z"/></svg>)SVG";
        case Symbol::Home:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-200h120v-240h240v240h120v-360L480-740 240-560v360Zm-80 80v-480l320-240 320 240v480H520v-240h-80v240H160Zm320-350Z"/></svg>)SVG";
        case Symbol::Cloud:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M260-160q-91 0-155.5-63T40-377q0-78 47-139t123-78q25-92 100-149t170-57q117 0 198.5 81.5T760-520q69 8 114.5 59.5T920-340q0 75-52.5 127.5T740-160H260Zm0-80h480q42 0 71-29t29-71q0-42-29-71t-71-29h-60v-80q0-83-58.5-141.5T480-720q-83 0-141.5 58.5T280-520h-20q-58 0-99 41t-41 99q0 58 41 99t99 41Zm220-240Z"/></svg>)SVG";
        case Symbol::AirDrop:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-304 304-480l176-176 176 176-176 176Zm56 199q-11 11-26 17t-30 6q-15 0-30-6t-26-17L105-424q-11-11-17-26t-6-30q0-15 6-30t17-26l318-318q12-12 26.5-18t30.5-6q16 0 30.5 6t26.5 18l318 318q11 11 17 26t6 30q0 15-6 30t-17 26L536-105Zm-56-87 288-288-288-288-288 288 288 288Z"/></svg>)SVG";
        case Symbol::Recents:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-120q-138 0-240.5-91.5T122-440h82q14 104 92.5 172T480-200q117 0 198.5-81.5T760-480q0-117-81.5-198.5T480-760q-69 0-129 32t-101 88h110v80H120v-240h80v94q51-64 124.5-99T480-840q75 0 140.5 28.5t114 77q48.5 48.5 77 114T840-480q0 75-28.5 140.5t-77 114q-48.5 48.5-114 77T480-120Zm112-192L440-464v-216h80v184l128 128-56 56Z"/></svg>)SVG";
        case Symbol::Shared:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M440-280h320v-22q0-45-44-71.5T600-400q-72 0-116 26.5T440-302v22Zm160-160q33 0 56.5-23.5T680-520q0-33-23.5-56.5T600-600q-33 0-56.5 23.5T520-520q0 33 23.5 56.5T600-440ZM160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h240l80 80h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160Zm0-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Z"/></svg>)SVG";
        case Symbol::Sidebar:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm280-80h280v-560H480v560Z"/></svg>)SVG";
        case Symbol::NewFolder:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M560-320h80v-80h80v-80h-80v-80h-80v80h-80v80h80v80ZM160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h240l80 80h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160Zm0-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Z"/></svg>)SVG";
        case Symbol::Applications:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-160q-33 0-56.5-23.5T160-240q0-33 23.5-56.5T240-320q33 0 56.5 23.5T320-240q0 33-23.5 56.5T240-160Zm240 0q-33 0-56.5-23.5T400-240q0-33 23.5-56.5T480-320q33 0 56.5 23.5T560-240q0 33-23.5 56.5T480-160Zm240 0q-33 0-56.5-23.5T640-240q0-33 23.5-56.5T720-320q33 0 56.5 23.5T800-240q0 33-23.5 56.5T720-160ZM240-400q-33 0-56.5-23.5T160-480q0-33 23.5-56.5T240-560q33 0 56.5 23.5T320-480q0 33-23.5 56.5T240-400Zm240 0q-33 0-56.5-23.5T400-480q0-33 23.5-56.5T480-560q33 0 56.5 23.5T560-480q0 33-23.5 56.5T480-400Zm240 0q-33 0-56.5-23.5T640-480q0-33 23.5-56.5T720-560q33 0 56.5 23.5T800-480q0 33-23.5 56.5T720-400ZM240-640q-33 0-56.5-23.5T160-720q0-33 23.5-56.5T240-800q33 0 56.5 23.5T320-720q0 33-23.5 56.5T240-640Zm240 0q-33 0-56.5-23.5T400-720q0-33 23.5-56.5T480-800q33 0 56.5 23.5T560-720q0 33-23.5 56.5T480-640Zm240 0q-33 0-56.5-23.5T640-720q0-33 23.5-56.5T720-800q33 0 56.5 23.5T800-720q0 33-23.5 56.5T720-640Z"/></svg>)SVG";
        case Symbol::Desktop:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M320-120v-40l80-80H160q-33 0-56.5-23.5T80-320v-440q0-33 23.5-56.5T160-840h640q33 0 56.5 23.5T880-760v440q0 33-23.5 56.5T800-240H560l80 80v40H320ZM160-440h640v-320H160v320Zm0 0v-320 320Z"/></svg>)SVG";
        case Symbol::Download:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-320 280-520l56-58 104 104v-326h80v326l104-104 56 58-200 200ZM240-160q-33 0-56.5-23.5T160-240v-120h80v120h480v-120h80v120q0 33-23.5 56.5T720-160H240Z"/></svg>)SVG";
        case Symbol::SortGroup:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M320-440v-287L217-624l-57-56 200-200 200 200-57 56-103-103v287h-80ZM600-80 400-280l57-56 103 103v-287h80v287l103-103 57 56L600-80Z"/></svg>)SVG";
        case Symbol::Duplicate:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M360-240q-33 0-56.5-23.5T280-320v-480q0-33 23.5-56.5T360-880h360q33 0 56.5 23.5T800-800v480q0 33-23.5 56.5T720-240H360Zm0-80h360v-480H360v480ZM200-80q-33 0-56.5-23.5T120-160v-560h80v560h440v80H200Zm160-240v-480 480Z"/></svg>)SVG";
        case Symbol::NewDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M440-240h80v-120h120v-80H520v-120h-80v120H320v80h120v120ZM240-80q-33 0-56.5-23.5T160-160v-640q0-33 23.5-56.5T240-880h320l240 240v480q0 33-23.5 56.5T720-80H240Zm280-520v-200H240v640h480v-440H520ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::PdfDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M360-460h40v-80h40q17 0 28.5-11.5T480-580v-40q0-17-11.5-28.5T440-660h-80v200Zm40-120v-40h40v40h-40Zm120 120h80q17 0 28.5-11.5T640-500v-120q0-17-11.5-28.5T600-660h-80v200Zm40-40v-120h40v120h-40Zm120 40h40v-80h40v-40h-40v-40h40v-40h-80v200ZM320-240q-33 0-56.5-23.5T240-320v-480q0-33 23.5-56.5T320-880h480q33 0 56.5 23.5T880-800v480q0 33-23.5 56.5T800-240H320Zm0-80h480v-480H320v480ZM160-80q-33 0-56.5-23.5T80-160v-560h80v560h560v80H160Zm160-720v480-480Z"/></svg>)SVG";
        case Symbol::TextDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M280-280h280v-80H280v80Zm0-160h400v-80H280v80Zm0-160h400v-80H280v80Zm-80 480q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm0-80h560v-560H200v560Zm0-560v560-560Z"/></svg>)SVG";
        case Symbol::Archive:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M640-480v-80h80v80h-80Zm0 80h-80v-80h80v80Zm0 80v-80h80v80h-80ZM447-640l-80-80H160v480h400v-80h80v80h160v-400H640v80h-80v-80H447ZM160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h240l80 80h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160Zm0-80v-480 480Z"/></svg>)SVG";
        case Symbol::AudioDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M430-200q38 0 64-26t26-64v-150h120v-80H480v155q-11-8-23.5-11.5T430-380q-38 0-64 26t-26 64q0 38 26 64t64 26ZM240-80q-33 0-56.5-23.5T160-160v-640q0-33 23.5-56.5T240-880h320l240 240v480q0 33-23.5 56.5T720-80H240Zm280-520v-200H240v640h480v-440H520ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::CodeDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M320-240 80-480l240-240 57 57-184 184 183 183-56 56Zm320 0-57-57 184-184-183-183 56-56 240 240-240 240Z"/></svg>)SVG";
        case Symbol::SpreadsheetDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M760-120H200q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120ZM200-640h560v-120H200v120Zm100 80H200v360h100v-360Zm360 0v360h100v-360H660Zm-80 0H380v360h200v-360Z"/></svg>)SVG";
        case Symbol::PresentationDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="m380-300 280-180-280-180v360ZM200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm0-80h560v-560H200v560Zm0-560v560-560Z"/></svg>)SVG";
        default:
            return {};
        }
    case MaterialSymbolsStyle::Rounded:
        switch (symbol) {
        case Symbol::Back:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="m432-480 156 156q11 11 11 28t-11 28q-11 11-28 11t-28-11L348-452q-6-6-8.5-13t-2.5-15q0-8 2.5-15t8.5-13l184-184q11-11 28-11t28 11q11 11 11 28t-11 28L432-480Z"/></svg>)SVG";
        case Symbol::Forward:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M504-480 348-636q-11-11-11-28t11-28q11-11 28-11t28 11l184 184q6 6 8.5 13t2.5 15q0 8-2.5 15t-8.5 13L404-268q-11 11-28 11t-28-11q-11-11-11-28t11-28l156-156Z"/></svg>)SVG";
        case Symbol::Search:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M380-320q-109 0-184.5-75.5T120-580q0-109 75.5-184.5T380-840q109 0 184.5 75.5T640-580q0 44-14 83t-38 69l224 224q11 11 11 28t-11 28q-11 11-28 11t-28-11L532-372q-30 24-69 38t-83 14Zm0-80q75 0 127.5-52.5T560-580q0-75-52.5-127.5T380-760q-75 0-127.5 52.5T200-580q0 75 52.5 127.5T380-400Z"/></svg>)SVG";
        case Symbol::Share:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-40q-33 0-56.5-23.5T160-120v-440q0-33 23.5-56.5T240-640h80q17 0 28.5 11.5T360-600q0 17-11.5 28.5T320-560h-80v440h480v-440h-80q-17 0-28.5-11.5T600-600q0-17 11.5-28.5T640-640h80q33 0 56.5 23.5T800-560v440q0 33-23.5 56.5T720-40H240Zm200-727-36 36q-12 12-28 11.5T348-732q-11-12-11.5-28t11.5-28l104-104q12-12 28-12t28 12l104 104q11 11 11 27.5T612-732q-12 12-28.5 12T555-732l-35-35v407q0 17-11.5 28.5T480-320q-17 0-28.5-11.5T440-360v-407Z"/></svg>)SVG";
        case Symbol::Tag:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M856-390 570-104q-12 12-27 18t-30 6q-15 0-30-6t-27-18L103-457q-11-11-17-25.5T80-513v-287q0-33 23.5-56.5T160-880h287q16 0 31 6.5t26 17.5l352 353q12 12 17.5 27t5.5 30q0 15-5.5 29.5T856-390ZM513-160l286-286-353-354H160v286l353 354ZM260-640q25 0 42.5-17.5T320-700q0-25-17.5-42.5T260-760q-25 0-42.5 17.5T200-700q0 25 17.5 42.5T260-640Zm220 160Z"/></svg>)SVG";
        case Symbol::More:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-400q-33 0-56.5-23.5T160-480q0-33 23.5-56.5T240-560q33 0 56.5 23.5T320-480q0 33-23.5 56.5T240-400Zm240 0q-33 0-56.5-23.5T400-480q0-33 23.5-56.5T480-560q33 0 56.5 23.5T560-480q0 33-23.5 56.5T480-400Zm240 0q-33 0-56.5-23.5T640-480q0-33 23.5-56.5T720-560q33 0 56.5 23.5T800-480q0 33-23.5 56.5T720-400Z"/></svg>)SVG";
        case Symbol::Grid:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M200-520q-33 0-56.5-23.5T120-600v-160q0-33 23.5-56.5T200-840h160q33 0 56.5 23.5T440-760v160q0 33-23.5 56.5T360-520H200Zm0 400q-33 0-56.5-23.5T120-200v-160q0-33 23.5-56.5T200-440h160q33 0 56.5 23.5T440-360v160q0 33-23.5 56.5T360-120H200Zm400-400q-33 0-56.5-23.5T520-600v-160q0-33 23.5-56.5T600-840h160q33 0 56.5 23.5T840-760v160q0 33-23.5 56.5T760-520H600Zm0 400q-33 0-56.5-23.5T520-200v-160q0-33 23.5-56.5T600-440h160q33 0 56.5 23.5T840-360v160q0 33-23.5 56.5T760-120H600ZM200-600h160v-160H200v160Zm400 0h160v-160H600v160Zm0 400h160v-160H600v160Zm-400 0h160v-160H200v160Zm400-400Zm0 240Zm-240 0Zm0-240Z"/></svg>)SVG";
        case Symbol::List:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M120-280v-400q0-33 23.5-56.5T200-760h560q33 0 56.5 23.5T840-680v400q0 33-23.5 56.5T760-200H200q-33 0-56.5-23.5T120-280Zm80-320h80v-80h-80v80Zm160 0h400v-80H360v80Zm0 160h400v-80H360v80Zm0 160h400v-80H360v80Zm-160 0h80v-80h-80v80Zm0-160h80v-80h-80v80Z"/></svg>)SVG";
        case Symbol::Columns:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M121-280v-400q0-33 23.5-56.5T201-760h559q33 0 56.5 23.5T840-680v400q0 33-23.5 56.5T760-200H201q-33 0-56.5-23.5T121-280Zm79 0h133v-400H200v400Zm213 0h133v-400H413v400Zm213 0h133v-400H626v400Z"/></svg>)SVG";
        case Symbol::Gallery:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M80-360v-240q0-33 23.5-56.5T160-680q33 0 56.5 23.5T240-600v240q0 33-23.5 56.5T160-280q-33 0-56.5-23.5T80-360Zm280 160q-33 0-56.5-23.5T280-280v-400q0-33 23.5-56.5T360-760h240q33 0 56.5 23.5T680-680v400q0 33-23.5 56.5T600-200H360Zm360-160v-240q0-33 23.5-56.5T800-680q33 0 56.5 23.5T880-600v240q0 33-23.5 56.5T800-280q-33 0-56.5-23.5T720-360Zm-360 80h240v-400H360v400Zm120-200Z"/></svg>)SVG";
        case Symbol::Folder:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h207q16 0 30.5 6t25.5 17l57 57h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160Zm0-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Z"/></svg>)SVG";
        case Symbol::Trash:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M280-120q-33 0-56.5-23.5T200-200v-520q-17 0-28.5-11.5T160-760q0-17 11.5-28.5T200-800h160q0-17 11.5-28.5T400-840h160q17 0 28.5 11.5T600-800h160q17 0 28.5 11.5T800-760q0 17-11.5 28.5T760-720v520q0 33-23.5 56.5T680-120H280Zm400-600H280v520h400v-520ZM400-280q17 0 28.5-11.5T440-320v-280q0-17-11.5-28.5T400-640q-17 0-28.5 11.5T360-600v280q0 17 11.5 28.5T400-280Zm160 0q17 0 28.5-11.5T600-320v-280q0-17-11.5-28.5T560-640q-17 0-28.5 11.5T520-600v280q0 17 11.5 28.5T560-280ZM280-720v520-520Z"/></svg>)SVG";
        case Symbol::Document:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-80q-33 0-56.5-23.5T160-160v-640q0-33 23.5-56.5T240-880h287q16 0 30.5 6t25.5 17l194 194q11 11 17 25.5t6 30.5v447q0 33-23.5 56.5T720-80H240Zm280-560v-160H240v640h480v-440H560q-17 0-28.5-11.5T520-640ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::Image:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm0-80h560v-560H200v560Zm0 0v-560 560Zm80-80h400q12 0 18-11t-2-21L586-459q-6-8-16-8t-16 8L450-320l-74-99q-6-8-16-8t-16 8l-80 107q-8 10-2 21t18 11Z"/></svg>)SVG";
        case Symbol::Movie:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="m160-800 65 130q7 14 20 22t28 8q30 0 46-25.5t2-52.5l-41-82h80l65 130q7 14 20 22t28 8q30 0 46-25.5t2-52.5l-41-82h80l65 130q7 14 20 22t28 8q30 0 46-25.5t2-52.5l-41-82h120q33 0 56.5 23.5T880-720v480q0 33-23.5 56.5T800-160H160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800Zm0 240v320h640v-320H160Zm0 0v320-320Z"/></svg>)SVG";
        case Symbol::Plus:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M440-440H240q-17 0-28.5-11.5T200-480q0-17 11.5-28.5T240-520h200v-200q0-17 11.5-28.5T480-760q17 0 28.5 11.5T520-720v200h200q17 0 28.5 11.5T760-480q0 17-11.5 28.5T720-440H520v200q0 17-11.5 28.5T480-200q-17 0-28.5-11.5T440-240v-200Z"/></svg>)SVG";
        case Symbol::XMark:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-424 284-228q-11 11-28 11t-28-11q-11-11-11-28t11-28l196-196-196-196q-11-11-11-28t11-28q11-11 28-11t28 11l196 196 196-196q11-11 28-11t28 11q11 11 11 28t-11 28L536-480l196 196q11 11 11 28t-11 28q-11 11-28 11t-28-11L480-424Z"/></svg>)SVG";
        case Symbol::ChevronDown:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-361q-8 0-15-2.5t-13-8.5L268-556q-11-11-11-28t11-28q11-11 28-11t28 11l156 156 156-156q11-11 28-11t28 11q11 11 11 28t-11 28L508-372q-6 6-13 8.5t-15 2.5Z"/></svg>)SVG";
        case Symbol::ChevronUp:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-528 324-372q-11 11-28 11t-28-11q-11-11-11-28t11-28l184-184q12-12 28-12t28 12l184 184q11 11 11 28t-11 28q-11 11-28 11t-28-11L480-528Z"/></svg>)SVG";
        case Symbol::Home:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-200h120v-200q0-17 11.5-28.5T400-440h160q17 0 28.5 11.5T600-400v200h120v-360L480-740 240-560v360Zm-80 0v-360q0-19 8.5-36t23.5-28l240-180q21-16 48-16t48 16l240 180q15 11 23.5 28t8.5 36v360q0 33-23.5 56.5T720-120H560q-17 0-28.5-11.5T520-160v-200h-80v200q0 17-11.5 28.5T400-120H240q-33 0-56.5-23.5T160-200Zm320-270Z"/></svg>)SVG";
        case Symbol::Cloud:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M260-160q-91 0-155.5-63T40-377q0-78 47-139t123-78q25-92 100-149t170-57q117 0 198.5 81.5T760-520q69 8 114.5 59.5T920-340q0 75-52.5 127.5T740-160H260Zm0-80h480q42 0 71-29t29-71q0-42-29-71t-71-29h-60v-80q0-83-58.5-141.5T480-720q-83 0-141.5 58.5T280-520h-20q-58 0-99 41t-41 99q0 58 41 99t99 41Zm220-240Z"/></svg>)SVG";
        case Symbol::AirDrop:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M452-332 332-452q-12-12-12-28t12-28l120-120q12-12 28-12t28 12l120 120q12 12 12 28t-12 28L508-332q-12 12-28 12t-28-12Zm84 227q-11 11-26 17t-30 6q-15 0-30-6t-26-17L105-424q-11-11-17-26t-6-30q0-15 6-30t17-26l318-318q12-12 26.5-18t30.5-6q16 0 30.5 6t26.5 18l318 318q11 11 17 26t6 30q0 15-6 30t-17 26L536-105Zm-56-87 288-288-288-288-288 288 288 288Z"/></svg>)SVG";
        case Symbol::Recents:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-120q-126 0-223-76.5T131-392q-4-15 6-27.5t27-14.5q16-2 29 6t18 24q24 90 99 147t170 57q117 0 198.5-81.5T760-480q0-117-81.5-198.5T480-760q-69 0-129 32t-101 88h70q17 0 28.5 11.5T360-600q0 17-11.5 28.5T320-560H160q-17 0-28.5-11.5T120-600v-160q0-17 11.5-28.5T160-800q17 0 28.5 11.5T200-760v54q51-64 124.5-99T480-840q75 0 140.5 28.5t114 77q48.5 48.5 77 114T840-480q0 75-28.5 140.5t-77 114q-48.5 48.5-114 77T480-120Zm40-376 100 100q11 11 11 28t-11 28q-11 11-28 11t-28-11L452-452q-6-6-9-13.5t-3-15.5v-159q0-17 11.5-28.5T480-680q17 0 28.5 11.5T520-640v144Z"/></svg>)SVG";
        case Symbol::Shared:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h207q16 0 30.5 6t25.5 17l57 57h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160Zm0-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Zm280-40h320v-22q0-45-44-71.5T600-400q-72 0-116 26.5T440-302v22Zm160-160q33 0 56.5-23.5T680-520q0-33-23.5-56.5T600-600q-33 0-56.5 23.5T520-520q0 33 23.5 56.5T600-440Z"/></svg>)SVG";
        case Symbol::Sidebar:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm280-80h280v-560H480v560Z"/></svg>)SVG";
        case Symbol::NewFolder:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h207q16 0 30.5 6t25.5 17l57 57h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160Zm0-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Zm400-160v40q0 17 11.5 28.5T600-320q17 0 28.5-11.5T640-360v-40h40q17 0 28.5-11.5T720-440q0-17-11.5-28.5T680-480h-40v-40q0-17-11.5-28.5T600-560q-17 0-28.5 11.5T560-520v40h-40q-17 0-28.5 11.5T480-440q0 17 11.5 28.5T520-400h40Z"/></svg>)SVG";
        case Symbol::Applications:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-160q-33 0-56.5-23.5T160-240q0-33 23.5-56.5T240-320q33 0 56.5 23.5T320-240q0 33-23.5 56.5T240-160Zm240 0q-33 0-56.5-23.5T400-240q0-33 23.5-56.5T480-320q33 0 56.5 23.5T560-240q0 33-23.5 56.5T480-160Zm240 0q-33 0-56.5-23.5T640-240q0-33 23.5-56.5T720-320q33 0 56.5 23.5T800-240q0 33-23.5 56.5T720-160ZM240-400q-33 0-56.5-23.5T160-480q0-33 23.5-56.5T240-560q33 0 56.5 23.5T320-480q0 33-23.5 56.5T240-400Zm240 0q-33 0-56.5-23.5T400-480q0-33 23.5-56.5T480-560q33 0 56.5 23.5T560-480q0 33-23.5 56.5T480-400Zm240 0q-33 0-56.5-23.5T640-480q0-33 23.5-56.5T720-560q33 0 56.5 23.5T800-480q0 33-23.5 56.5T720-400ZM240-640q-33 0-56.5-23.5T160-720q0-33 23.5-56.5T240-800q33 0 56.5 23.5T320-720q0 33-23.5 56.5T240-640Zm240 0q-33 0-56.5-23.5T400-720q0-33 23.5-56.5T480-800q33 0 56.5 23.5T560-720q0 33-23.5 56.5T480-640Zm240 0q-33 0-56.5-23.5T640-720q0-33 23.5-56.5T720-800q33 0 56.5 23.5T800-720q0 33-23.5 56.5T720-640Z"/></svg>)SVG";
        case Symbol::Desktop:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M400-240H160q-33 0-56.5-23.5T80-320v-440q0-33 23.5-56.5T160-840h640q33 0 56.5 23.5T880-760v440q0 33-23.5 56.5T800-240H560l74 74q2 2 6 14v12q0 8-6 14t-14 6H334q-6 0-10-4t-4-10v-20q0-2 4-10l76-76ZM160-440h640v-320H160v320Zm0 0v-320 320Z"/></svg>)SVG";
        case Symbol::Download:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-337q-8 0-15-2.5t-13-8.5L308-492q-12-12-11.5-28t11.5-28q12-12 28.5-12.5T365-549l75 75v-286q0-17 11.5-28.5T480-800q17 0 28.5 11.5T520-760v286l75-75q12-12 28.5-11.5T652-548q11 12 11.5 28T652-492L508-348q-6 6-13 8.5t-15 2.5ZM240-160q-33 0-56.5-23.5T160-240v-80q0-17 11.5-28.5T200-360q17 0 28.5 11.5T240-320v80h480v-80q0-17 11.5-28.5T760-360q17 0 28.5 11.5T800-320v80q0 33-23.5 56.5T720-160H240Z"/></svg>)SVG";
        case Symbol::SortGroup:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M360-440q-17 0-28.5-11.5T320-480v-247l-75 75q-11 11-27.5 11T189-652q-12-12-12-28.5t12-28.5l143-143q6-6 13-8.5t15-2.5q8 0 15 2.5t13 8.5l144 144q12 12 11.5 28T531-652q-12 11-28 11.5T475-652l-75-75v247q0 17-11.5 28.5T360-440ZM600-97q-8 0-15-2.5t-13-8.5L428-252q-12-12-11.5-28t12.5-28q12-11 28-11.5t28 11.5l75 75v-247q0-17 11.5-28.5T600-520q17 0 28.5 11.5T640-480v247l75-75q11-11 27.5-11t28.5 11q12 12 12 28.5T771-251L628-108q-6 6-13 8.5T600-97Z"/></svg>)SVG";
        case Symbol::Duplicate:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M360-240q-33 0-56.5-23.5T280-320v-480q0-33 23.5-56.5T360-880h360q33 0 56.5 23.5T800-800v480q0 33-23.5 56.5T720-240H360Zm0-80h360v-480H360v480ZM200-80q-33 0-56.5-23.5T120-160v-520q0-17 11.5-28.5T160-720q17 0 28.5 11.5T200-680v520h400q17 0 28.5 11.5T640-120q0 17-11.5 28.5T600-80H200Zm160-240v-480 480Z"/></svg>)SVG";
        case Symbol::NewDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M440-360v80q0 17 11.5 28.5T480-240q17 0 28.5-11.5T520-280v-80h80q17 0 28.5-11.5T640-400q0-17-11.5-28.5T600-440h-80v-80q0-17-11.5-28.5T480-560q-17 0-28.5 11.5T440-520v80h-80q-17 0-28.5 11.5T320-400q0 17 11.5 28.5T360-360h80ZM240-80q-33 0-56.5-23.5T160-160v-640q0-33 23.5-56.5T240-880h287q16 0 30.5 6t25.5 17l194 194q11 11 17 25.5t6 30.5v447q0 33-23.5 56.5T720-80H240Zm280-560v-160H240v640h480v-440H560q-17 0-28.5-11.5T520-640ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::PdfDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M400-540h40q17 0 28.5-11.5T480-580v-40q0-17-11.5-28.5T440-660h-60q-8 0-14 6t-6 14v160q0 8 6 14t14 6q8 0 14-6t6-14v-60Zm0-40v-40h40v40h-40Zm200 120q17 0 28.5-11.5T640-500v-120q0-17-11.5-28.5T600-660h-60q-8 0-14 6t-6 14v160q0 8 6 14t14 6h60Zm-40-40v-120h40v120h-40Zm160-40h20q8 0 14-6t6-14q0-8-6-14t-14-6h-20v-40h20q8 0 14-6t6-14q0-8-6-14t-14-6h-40q-8 0-14 6t-6 14v160q0 8 6 14t14 6q8 0 14-6t6-14v-60ZM320-240q-33 0-56.5-23.5T240-320v-480q0-33 23.5-56.5T320-880h480q33 0 56.5 23.5T880-800v480q0 33-23.5 56.5T800-240H320Zm0-80h480v-480H320v480ZM160-80q-33 0-56.5-23.5T80-160v-520q0-17 11.5-28.5T120-720q17 0 28.5 11.5T160-680v520h520q17 0 28.5 11.5T720-120q0 17-11.5 28.5T680-80H160Zm160-720v480-480Z"/></svg>)SVG";
        case Symbol::TextDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm0-80h560v-560H200v560Zm0-560v560-560Zm120 480h200q17 0 28.5-11.5T560-320q0-17-11.5-28.5T520-360H320q-17 0-28.5 11.5T280-320q0 17 11.5 28.5T320-280Zm0-160h320q17 0 28.5-11.5T680-480q0-17-11.5-28.5T640-520H320q-17 0-28.5 11.5T280-480q0 17 11.5 28.5T320-440Zm0-160h320q17 0 28.5-11.5T680-640q0-17-11.5-28.5T640-680H320q-17 0-28.5 11.5T280-640q0 17 11.5 28.5T320-600Z"/></svg>)SVG";
        case Symbol::Archive:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M640-480v-80h80v80h-80Zm0 80h-80v-80h80v80Zm0 80v-80h80v80h-80ZM447-640l-80-80H160v480h400v-80h80v80h160v-400H640v80h-80v-80H447ZM160-160q-33 0-56.5-23.5T80-240v-480q0-33 23.5-56.5T160-800h207q16 0 30.5 6t25.5 17l57 57h320q33 0 56.5 23.5T880-640v400q0 33-23.5 56.5T800-160H160Zm0-80v-480 480Z"/></svg>)SVG";
        case Symbol::AudioDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M430-200q38 0 64-26t26-64v-150h80q17 0 28.5-11.5T640-480q0-17-11.5-28.5T600-520h-80q-17 0-28.5 11.5T480-480v115q-11-8-23.5-11.5T430-380q-38 0-64 26t-26 64q0 38 26 64t64 26ZM240-80q-33 0-56.5-23.5T160-160v-640q0-33 23.5-56.5T240-880h287q16 0 30.5 6t25.5 17l194 194q11 11 17 25.5t6 30.5v447q0 33-23.5 56.5T720-80H240Zm280-560v-160H240v640h480v-440H560q-17 0-28.5-11.5T520-640ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::CodeDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="m193-479 155 155q11 11 11 28t-11 28q-11 11-28 11t-28-11L108-452q-6-6-8.5-13T97-480q0-8 2.5-15t8.5-13l184-184q12-12 28.5-12t28.5 12q12 12 12 28.5T349-635L193-479Zm574-2L612-636q-11-11-11-28t11-28q11-11 28-11t28 11l184 184q6 6 8.5 13t2.5 15q0 8-2.5 15t-8.5 13L668-268q-12 12-28 11.5T612-269q-12-12-12-28.5t12-28.5l155-155Z"/></svg>)SVG";
        case Symbol::SpreadsheetDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M760-120H200q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120ZM200-640h560v-120H200v120Zm100 80H200v360h100v-360Zm360 0v360h100v-360H660Zm-80 0H380v360h200v-360Z"/></svg>)SVG";
        case Symbol::PresentationDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M634-463q9-6 9-17t-9-17L411-640q-10-7-20.5-1T380-623v286q0 12 10.5 18t20.5-1l223-143ZM200-120q-33 0-56.5-23.5T120-200v-560q0-33 23.5-56.5T200-840h560q33 0 56.5 23.5T840-760v560q0 33-23.5 56.5T760-120H200Zm0-80h560v-560H200v560Zm0-560v560-560Z"/></svg>)SVG";
        default:
            return {};
        }
    case MaterialSymbolsStyle::Sharp:
        switch (symbol) {
        case Symbol::Back:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M560-240 320-480l240-240 56 56-184 184 184 184-56 56Z"/></svg>)SVG";
        case Symbol::Forward:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M504-480 320-664l56-56 240 240-240 240-56-56 184-184Z"/></svg>)SVG";
        case Symbol::Search:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M784-120 532-372q-30 24-69 38t-83 14q-109 0-184.5-75.5T120-580q0-109 75.5-184.5T380-840q109 0 184.5 75.5T640-580q0 44-14 83t-38 69l252 252-56 56ZM380-400q75 0 127.5-52.5T560-580q0-75-52.5-127.5T380-760q-75 0-127.5 52.5T200-580q0 75 52.5 127.5T380-400Z"/></svg>)SVG";
        case Symbol::Share:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M160-40v-600h200v80H240v440h480v-440H600v-80h200v600H160Zm280-280v-447l-64 64-56-57 160-160 160 160-56 57-64-64v447h-80Z"/></svg>)SVG";
        case Symbol::Tag:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M513-47 80-480v-400h400l432 434L513-47Zm0-113 286-286-353-354H160v286l353 354ZM260-640q25 0 42.5-17.5T320-700q0-25-17.5-42.5T260-760q-25 0-42.5 17.5T200-700q0 25 17.5 42.5T260-640Zm220 160Z"/></svg>)SVG";
        case Symbol::More:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-400q-33 0-56.5-23.5T160-480q0-33 23.5-56.5T240-560q33 0 56.5 23.5T320-480q0 33-23.5 56.5T240-400Zm240 0q-33 0-56.5-23.5T400-480q0-33 23.5-56.5T480-560q33 0 56.5 23.5T560-480q0 33-23.5 56.5T480-400Zm240 0q-33 0-56.5-23.5T640-480q0-33 23.5-56.5T720-560q33 0 56.5 23.5T800-480q0 33-23.5 56.5T720-400Z"/></svg>)SVG";
        case Symbol::Grid:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M120-520v-320h320v320H120Zm0 400v-320h320v320H120Zm400-400v-320h320v320H520Zm0 400v-320h320v320H520ZM200-600h160v-160H200v160Zm400 0h160v-160H600v160Zm0 400h160v-160H600v160Zm-400 0h160v-160H200v160Zm400-400Zm0 240Zm-240 0Zm0-240Z"/></svg>)SVG";
        case Symbol::List:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M360-240h440v-107H360v107ZM160-613h120v-107H160v107Zm0 187h120v-107H160v107Zm0 186h120v-107H160v107Zm200-186h440v-107H360v107Zm0-187h440v-107H360v107ZM80-160v-640h800v640H80Z"/></svg>)SVG";
        case Symbol::Columns:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M121-200v-560h719v560H121Zm79-80h133v-400H200v400Zm213 0h133v-400H413v400Zm213 0h133v-400H626v400Z"/></svg>)SVG";
        case Symbol::Gallery:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M80-280v-400h160v400H80Zm200 80v-560h400v560H280Zm440-80v-400h160v400H720Zm-360 0h240v-400H360v400Zm120-200Z"/></svg>)SVG";
        case Symbol::Folder:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M80-160v-640h320l80 80h400v560H80Zm80-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Z"/></svg>)SVG";
        case Symbol::Trash:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M200-120v-600h-40v-80h200v-40h240v40h200v80h-40v600H200Zm80-80h400v-520H280v520Zm80-80h80v-360h-80v360Zm160 0h80v-360h-80v360ZM280-720v520-520Z"/></svg>)SVG";
        case Symbol::Document:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M160-80v-800h400l240 240v560H160Zm360-520v-200H240v640h480v-440H520ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::Image:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-280h480L570-480 450-320l-90-120-120 160ZM120-120v-720h720v720H120Zm80-80h560v-560H200v560Zm0 0v-560 560Z"/></svg>)SVG";
        case Symbol::Movie:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M80-160v-640h80l80 160h120l-80-160h80l80 160h120l-80-160h80l80 160h120l-80-160h200v640H80Zm80-400v320h640v-320H160Zm0 0v320-320Z"/></svg>)SVG";
        case Symbol::Plus:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M440-440H200v-80h240v-240h80v240h240v80H520v240h-80v-240Z"/></svg>)SVG";
        case Symbol::XMark:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="m256-200-56-56 224-224-224-224 56-56 224 224 224-224 56 56-224 224 224 224-56 56-224-224-224 224Z"/></svg>)SVG";
        case Symbol::ChevronDown:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-344 240-584l56-56 184 184 184-184 56 56-240 240Z"/></svg>)SVG";
        case Symbol::ChevronUp:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-528 296-344l-56-56 240-240 240 240-56 56-184-184Z"/></svg>)SVG";
        case Symbol::Home:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-200h120v-240h240v240h120v-360L480-740 240-560v360Zm-80 80v-480l320-240 320 240v480H520v-240h-80v240H160Zm320-350Z"/></svg>)SVG";
        case Symbol::Cloud:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M260-160q-91 0-155.5-63T40-377q0-78 47-139t123-78q25-92 100-149t170-57q117 0 198.5 81.5T760-520q69 8 114.5 59.5T920-340q0 75-52.5 127.5T740-160H260Zm0-80h480q42 0 71-29t29-71q0-42-29-71t-71-29h-60v-80q0-83-58.5-141.5T480-720q-83 0-141.5 58.5T280-520h-20q-58 0-99 41t-41 99q0 58 41 99t99 41Zm220-240Z"/></svg>)SVG";
        case Symbol::AirDrop:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-304 304-480l176-176 176 176-176 176Zm0 255L49-480l431-431 431 431L480-49Zm0-143 288-288-288-288-288 288 288 288Z"/></svg>)SVG";
        case Symbol::Recents:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-120q-138 0-240.5-91.5T122-440h82q14 104 92.5 172T480-200q117 0 198.5-81.5T760-480q0-117-81.5-198.5T480-760q-69 0-129 32t-101 88h110v80H120v-240h80v94q51-64 124.5-99T480-840q75 0 140.5 28.5t114 77q48.5 48.5 77 114T840-480q0 75-28.5 140.5t-77 114q-48.5 48.5-114 77T480-120Zm112-192L440-464v-216h80v184l128 128-56 56Z"/></svg>)SVG";
        case Symbol::Shared:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M80-160v-640h320l80 80h400v560H80Zm80-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Zm280-40h320v-22q0-45-44-71.5T600-400q-72 0-116 26.5T440-302v22Zm160-160q33 0 56.5-23.5T680-520q0-33-23.5-56.5T600-600q-33 0-56.5 23.5T520-520q0 33 23.5 56.5T600-440Z"/></svg>)SVG";
        case Symbol::Sidebar:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M120-120v-720h720v720H120Zm360-80h280v-560H480v560Z"/></svg>)SVG";
        case Symbol::NewFolder:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M80-160v-640h320l80 80h400v560H80Zm80-80h640v-400H447l-80-80H160v480Zm0 0v-480 480Zm400-80h80v-80h80v-80h-80v-80h-80v80h-80v80h80v80Z"/></svg>)SVG";
        case Symbol::Applications:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M240-160q-33 0-56.5-23.5T160-240q0-33 23.5-56.5T240-320q33 0 56.5 23.5T320-240q0 33-23.5 56.5T240-160Zm240 0q-33 0-56.5-23.5T400-240q0-33 23.5-56.5T480-320q33 0 56.5 23.5T560-240q0 33-23.5 56.5T480-160Zm240 0q-33 0-56.5-23.5T640-240q0-33 23.5-56.5T720-320q33 0 56.5 23.5T800-240q0 33-23.5 56.5T720-160ZM240-400q-33 0-56.5-23.5T160-480q0-33 23.5-56.5T240-560q33 0 56.5 23.5T320-480q0 33-23.5 56.5T240-400Zm240 0q-33 0-56.5-23.5T400-480q0-33 23.5-56.5T480-560q33 0 56.5 23.5T560-480q0 33-23.5 56.5T480-400Zm240 0q-33 0-56.5-23.5T640-480q0-33 23.5-56.5T720-560q33 0 56.5 23.5T800-480q0 33-23.5 56.5T720-400ZM240-640q-33 0-56.5-23.5T160-720q0-33 23.5-56.5T240-800q33 0 56.5 23.5T320-720q0 33-23.5 56.5T240-640Zm240 0q-33 0-56.5-23.5T400-720q0-33 23.5-56.5T480-800q33 0 56.5 23.5T560-720q0 33-23.5 56.5T480-640Zm240 0q-33 0-56.5-23.5T640-720q0-33 23.5-56.5T720-800q33 0 56.5 23.5T800-720q0 33-23.5 56.5T720-640Z"/></svg>)SVG";
        case Symbol::Desktop:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M320-120v-40l80-80H80v-600h800v600H560l80 80v40H320ZM160-440h640v-320H160v320Zm0 0v-320 320Z"/></svg>)SVG";
        case Symbol::Download:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M480-320 280-520l56-58 104 104v-326h80v326l104-104 56 58-200 200ZM160-160v-200h80v120h480v-120h80v200H160Z"/></svg>)SVG";
        case Symbol::SortGroup:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M320-440v-287L217-624l-57-56 200-200 200 200-57 56-103-103v287h-80ZM600-80 400-280l57-56 103 103v-287h80v287l103-103 57 56L600-80Z"/></svg>)SVG";
        case Symbol::Duplicate:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M280-240v-640h520v640H280Zm80-80h360v-480H360v480ZM120-80v-640h80v560h440v80H120Zm240-240v-480 480Z"/></svg>)SVG";
        case Symbol::NewDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M440-240h80v-120h120v-80H520v-120h-80v120H320v80h120v120ZM160-80v-800h400l240 240v560H160Zm360-520v-200H240v640h480v-440H520ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::PdfDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M360-460h40v-80h60l20-20v-80l-20-20H360v200Zm40-120v-40h40v40h-40Zm120 120h100l20-20v-160l-20-20H520v200Zm40-40v-120h40v120h-40Zm120 40h40v-80h40v-40h-40v-40h40v-40h-80v200ZM240-240v-640h640v640H240Zm80-80h480v-480H320v480ZM80-80v-640h80v560h560v80H80Zm240-720v480-480Z"/></svg>)SVG";
        case Symbol::TextDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M280-280h280v-80H280v80Zm0-160h400v-80H280v80Zm0-160h400v-80H280v80ZM120-120v-720h720v720H120Zm80-80h560v-560H200v560Zm0 0v-560 560Z"/></svg>)SVG";
        case Symbol::Archive:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M640-480v-80h80v80h-80Zm0 80h-80v-80h80v80Zm0 80v-80h80v80h-80ZM447-640l-80-80H160v480h400v-80h80v80h160v-400H640v80h-80v-80H447ZM80-160v-640h320l80 80h400v560H80Zm80-80v-480 480Z"/></svg>)SVG";
        case Symbol::AudioDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M430-200q38 0 64-26t26-64v-150h120v-80H480v155q-11-8-23.5-11.5T430-380q-38 0-64 26t-26 64q0 38 26 64t64 26ZM160-80v-800h400l240 240v560H160Zm360-520v-200H240v640h480v-440H520ZM240-800v200-200 640-640Z"/></svg>)SVG";
        case Symbol::CodeDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M320-240 80-480l240-240 57 57-184 184 183 183-56 56Zm320 0-57-57 184-184-183-183 56-56 240 240-240 240Z"/></svg>)SVG";
        case Symbol::SpreadsheetDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="M120-120v-720h720v720H120Zm80-520h560v-120H200v120Zm0 440h100v-360H200v360Zm460 0h100v-360H660v360Zm-280 0h200v-360H380v360Z"/></svg>)SVG";
        case Symbol::PresentationDocument:
            return R"SVG(<svg fill="currentColor" xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24"><path d="m380-300 280-180-280-180v360ZM120-120v-720h720v720H120Zm80-80h560v-560H200v560Zm0 0v-560 560Z"/></svg>)SVG";
        default:
            return {};
        }
    }
    return {};
}

inline auto material_symbols_svg_source(Symbol symbol) noexcept
        -> std::string_view {
    return material_symbols_svg_source(symbol, default_material_symbols_style());
}

inline auto svg_source(Symbol symbol,
                       MaterialSymbolsStyle style) noexcept
        -> std::string_view {
    return material_symbols_svg_source(symbol, style);
}

inline auto svg_source(Symbol symbol) noexcept -> std::string_view {
    return svg_source(symbol, default_material_symbols_style());
}

inline bool supports_hierarchical_opacity(Symbol) noexcept {
    return false;
}

inline auto symbol_layer_count(Symbol) noexcept -> unsigned int {
    return 1;
}

inline bool uses_svg_path_arcs(Symbol) noexcept {
    return false;
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

inline auto descriptor(Symbol symbol,
                       MaterialSymbolsStyle style) noexcept -> SymbolDescriptor {
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

    bool const filled = true;
    bool const hierarchical = supports_hierarchical_opacity(symbol);
    float const stroke_width = 0.0f;
    return SymbolDescriptor{
        symbol,
        name(symbol),
        role,
        SymbolVariant::Outline,
        hierarchical
            ? SymbolRenderingMode::Hierarchical
            : SymbolRenderingMode::Monochrome,
        SymbolScale::Medium,
        SymbolWeight::Regular,
        style_name(style),
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
        false,
        filled,
        true,
        true,
        hierarchical,
        false,
        false,
        !uses_material_symbols_source(symbol),
        false,
    };
}

inline auto descriptor(Symbol symbol) noexcept -> SymbolDescriptor {
    return descriptor(symbol, default_material_symbols_style());
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
    case SymbolTone::Disabled:    return {142, 142, 147, 255};
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
                ? SymbolColor{236, 236, 240, 150}
                : transparent,
            state.selected
                ? SymbolColor{232, 232, 236, 176}
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
                ? SymbolColor{226, 226, 232, 196}
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
        return 0.78f;
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

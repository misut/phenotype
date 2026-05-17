module;
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

inline constexpr unsigned int all_symbol_count = 31;
inline constexpr unsigned int sidebar_symbol_count = 11;
inline constexpr unsigned int toolbar_symbol_count = 15;
inline constexpr unsigned int outline_symbol_count = 30;
inline constexpr unsigned int filled_symbol_count = 1;
inline constexpr unsigned int hierarchical_symbol_count = 20;
inline constexpr unsigned int reference_symbol_count = all_symbol_count;

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

inline auto style_name() noexcept -> std::string_view {
    return "macos_rounded_outline_svg";
}

inline auto style_reference() noexcept -> std::string_view {
    return "Apple HIG, macOS Finder, and SF Symbols inspired custom rounded-outline SVG glyphs";
}

inline auto asset_policy() noexcept -> std::string_view {
    return "phenotype-owned vector assets; no Apple or SF Symbols artwork embedded";
}

inline auto reference_family() noexcept -> std::string_view {
    return "SF Symbols semantic reference";
}

inline auto reference_policy() noexcept -> std::string_view {
    return "semantic reference only; phenotype-owned SVG artwork";
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

inline auto svg_arc_policy() noexcept -> std::string_view {
    return "circle elements preserve native ArcTo; path A/a lowers to bounded cubic Bezier segments";
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

inline auto default_scale_policy() noexcept -> std::string_view {
    return "medium";
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
    case 18: return Symbol::Home;
    case 19: return Symbol::Cloud;
    case 20: return Symbol::AirDrop;
    case 21: return Symbol::Recents;
    case 22: return Symbol::Shared;
    case 23: return Symbol::Sidebar;
    case 24: return Symbol::NewFolder;
    case 25: return Symbol::Applications;
    case 26: return Symbol::Desktop;
    case 27: return Symbol::Download;
    case 28: return Symbol::SortGroup;
    case 29: return Symbol::Duplicate;
    case 30: return Symbol::NewDocument;
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
    }
    return "doc";
}

inline bool supports_hierarchical_opacity(Symbol symbol) noexcept {
    switch (symbol) {
    case Symbol::Share:
    case Symbol::Tag:
    case Symbol::List:
    case Symbol::Columns:
    case Symbol::Gallery:
    case Symbol::Trash:
    case Symbol::Document:
    case Symbol::Image:
    case Symbol::Movie:
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
    case Symbol::Grid:        return 4;
    case Symbol::List:        return 6;
    case Symbol::Trash:       return 5;
    case Symbol::Movie:       return 5;
    case Symbol::Sidebar:     return 4;
    case Symbol::SortGroup:   return 9;
    case Symbol::Document:
    case Symbol::AirDrop:
    case Symbol::Applications:
    case Symbol::NewDocument:
        return 4;
    case Symbol::Share:
    case Symbol::Columns:
    case Symbol::Gallery:
    case Symbol::Image:
    case Symbol::Home:
    case Symbol::Shared:
    case Symbol::NewFolder:
    case Symbol::Desktop:
    case Symbol::Download:
        return 3;
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

inline auto descriptor(Symbol symbol) noexcept -> SymbolDescriptor {
    SymbolRole role = SymbolRole::Toolbar;
    switch (symbol) {
    case Symbol::Back:
    case Symbol::Forward:
    case Symbol::ChevronDown:
        role = SymbolRole::Navigation;
        break;
    case Symbol::Folder:
    case Symbol::Document:
    case Symbol::Image:
    case Symbol::Movie:
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
    return SymbolDescriptor{
        symbol,
        name(symbol),
        role,
        filled ? SymbolVariant::Fill : SymbolVariant::Outline,
        hierarchical
            ? SymbolRenderingMode::Hierarchical
            : SymbolRenderingMode::Monochrome,
        SymbolScale::Medium,
        style_name(),
        semantic_reference_name(symbol),
        reference_family(),
        reference_policy(),
        24.0f,
        filled ? 0.0f : 1.8f,
        hierarchical ? 0.66f : 1.0f,
        symbol_layer_count(symbol),
        true,
        !filled,
        filled,
        true,
        hierarchical,
        true,
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

inline auto macos_file_type_color(Symbol symbol) noexcept -> SymbolColor {
    switch (symbol) {
    case Symbol::Folder:
    case Symbol::NewFolder:
        return {64, 156, 255, 255};
    case Symbol::Image:
        return {48, 176, 199, 255};
    case Symbol::Movie:
        return {88, 86, 214, 255};
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

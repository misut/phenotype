module;
#include <cstdint>
#include <cstring>
#include <string_view>
export module phenotype.icons;

import phenotype.svg;
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

inline auto name(Symbol symbol) noexcept -> std::string_view {
    switch (symbol) {
    case Symbol::Back:        return "back";
    case Symbol::Forward:     return "forward";
    case Symbol::Search:      return "search";
    case Symbol::Share:       return "share";
    case Symbol::Tag:         return "tag";
    case Symbol::More:        return "more";
    case Symbol::Grid:        return "grid";
    case Symbol::List:        return "list";
    case Symbol::Columns:     return "columns";
    case Symbol::Gallery:     return "gallery";
    case Symbol::Folder:      return "folder";
    case Symbol::Trash:       return "trash";
    case Symbol::Document:    return "document";
    case Symbol::Image:       return "image";
    case Symbol::Movie:       return "movie";
    case Symbol::Plus:        return "plus";
    case Symbol::XMark:       return "xmark";
    case Symbol::ChevronDown: return "chevron_down";
    case Symbol::Home:        return "home";
    case Symbol::Cloud:       return "cloud";
    case Symbol::AirDrop:     return "airdrop";
    case Symbol::Recents:     return "recents";
    case Symbol::Shared:      return "shared";
    case Symbol::Sidebar:     return "sidebar";
    case Symbol::NewFolder:   return "new_folder";
    case Symbol::Applications: return "applications";
    case Symbol::Desktop:     return "desktop";
    case Symbol::Download:    return "download";
    case Symbol::SortGroup:   return "sort_group";
    case Symbol::Duplicate:   return "duplicate";
    case Symbol::NewDocument: return "new_document";
    }
    return "unknown";
}

enum class SymbolRole {
    Navigation,
    Toolbar,
    Sidebar,
    FileType,
    Action,
};

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

struct SymbolDescriptor {
    Symbol symbol = Symbol::Document;
    std::string_view name;
    SymbolRole role = SymbolRole::Toolbar;
    std::string_view style;
    float grid_size = 24.0f;
    float default_stroke_width = 1.8f;
    bool uses_current_color = true;
    bool round_stroke = true;
    bool filled = false;
    bool phenotype_owned = true;
    bool uses_sf_symbols_asset = false;
};

inline constexpr unsigned int all_symbol_count = 31;
inline constexpr unsigned int sidebar_symbol_count = 11;
inline constexpr unsigned int toolbar_symbol_count = 15;

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

inline auto style_name() noexcept -> std::string_view {
    return "macos_rounded_outline_svg";
}

inline auto style_reference() noexcept -> std::string_view {
    return "Apple HIG Icons and SF Symbols inspired custom rounded-outline SVG glyphs";
}

inline auto asset_policy() noexcept -> std::string_view {
    return "phenotype-owned vector assets; no Apple or SF Symbols artwork embedded";
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
    return SymbolDescriptor{
        symbol,
        name(symbol),
        role,
        style_name(),
        24.0f,
        filled ? 0.0f : 1.8f,
        true,
        !filled,
        filled,
        true,
        false,
    };
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
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M12 4 L12 15"/><path d="M8 8 L12 4 L16 8"/><path d="M6 12 L6 20 L18 20 L18 12"/></svg>)SVG";
    case Symbol::Tag:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M4 12 L12 4 L20 12 L12 20 Z"/><circle cx="12" cy="9" r="1.2"/></svg>)SVG";
    case Symbol::More:
        return R"SVG(<svg viewBox="0 0 24 24" fill="currentColor" stroke="none"><circle cx="6" cy="12" r="1.5"/><circle cx="12" cy="12" r="1.5"/><circle cx="18" cy="12" r="1.5"/></svg>)SVG";
    case Symbol::Grid:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="4" width="6" height="6" rx="1.4"/><rect x="14" y="4" width="6" height="6" rx="1.4"/><rect x="4" y="14" width="6" height="6" rx="1.4"/><rect x="14" y="14" width="6" height="6" rx="1.4"/></svg>)SVG";
    case Symbol::List:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><path d="M8 6 L20 6"/><path d="M8 12 L20 12"/><path d="M8 18 L20 18"/><circle cx="4.5" cy="6" r="1"/><circle cx="4.5" cy="12" r="1"/><circle cx="4.5" cy="18" r="1"/></svg>)SVG";
    case Symbol::Columns:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="14" rx="2.4"/><path d="M9.3 5 L9.3 19"/><path d="M14.7 5 L14.7 19"/></svg>)SVG";
    case Symbol::Gallery:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="11" rx="2.4"/><path d="M7 20 L17 20"/><path d="M8 16 L16 16"/></svg>)SVG";
    case Symbol::Folder:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M4 8.5 L9.1 8.5 L11.1 10.4 L20 10.4 L20 19 L4 19 Z"/></svg>)SVG";
    case Symbol::Trash:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 8 L17 8"/><path d="M10 5 L14 5 L15 8"/><path d="M8 8 L9 20 L15 20 L16 8"/><path d="M11 11 L11 17"/><path d="M13 11 L13 17"/></svg>)SVG";
    case Symbol::Document:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 4 L14 4 L18 8 L18 20 L7 20 Z"/><path d="M14 4 L14 8 L18 8"/><path d="M9.5 13 L15.5 13"/><path d="M9.5 16 L15.5 16"/></svg>)SVG";
    case Symbol::Image:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="14" rx="2.4"/><circle cx="9" cy="10" r="1.5"/><polyline points="6.5 17 11 12.5 14 15.5 16 13.5 19 17"/></svg>)SVG";
    case Symbol::Movie:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="6" width="16" height="12" rx="2.4"/><path d="M8 6 L8 18"/><path d="M16 6 L16 18"/><path d="M4 10 L20 10"/><path d="M4 14 L20 14"/></svg>)SVG";
    case Symbol::Plus:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 5 L12 19"/><path d="M5 12 L19 12"/></svg>)SVG";
    case Symbol::XMark:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6.5 6.5 L17.5 17.5"/><path d="M17.5 6.5 L6.5 17.5"/></svg>)SVG";
    case Symbol::ChevronDown:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M6.5 9 L12 14.5 L17.5 9"/></svg>)SVG";
    case Symbol::Home:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M4 11.5 L12 5 L20 11.5"/><path d="M6.5 10 L6.5 20 L17.5 20 L17.5 10"/><path d="M10 20 L10 14 L14 14 L14 20"/></svg>)SVG";
    case Symbol::Cloud:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 18 L17 18 C20 18 21.5 16 21.5 13.7 C21.5 11.5 19.8 9.8 17.6 9.7 C16.8 7.3 14.7 6 12.2 6 C9.4 6 7.3 7.8 6.7 10.3 C4.4 10.7 2.5 12.2 2.5 14.4 C2.5 16.4 4.1 18 7 18 Z"/></svg>)SVG";
    case Symbol::AirDrop:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="13" r="1.8"/><path d="M8.5 16.8 Q12 13.6 15.5 16.8"/><path d="M5.8 19.2 Q12 13.2 18.2 19.2"/><path d="M4.5 9.5 Q12 3.5 19.5 9.5"/></svg>)SVG";
    case Symbol::Recents:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="8"/><path d="M12 7 L12 12 L8.5 12"/></svg>)SVG";
    case Symbol::Shared:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M3.8 8.4 L8.7 8.4 L10.5 10.1 L19.4 10.1 L19.4 18.2 L3.8 18.2 Z"/><circle cx="17.2" cy="7.6" r="2.2"/><path d="M13.8 12.5 C14.5 11.2 15.7 10.6 17.2 10.6 C18.7 10.6 20 11.2 20.6 12.5"/></svg>)SVG";
    case Symbol::Sidebar:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="14" rx="2.4"/><path d="M9 5 L9 19"/><path d="M6.2 8 L7.1 8"/><path d="M6.2 11 L7.1 11"/></svg>)SVG";
    case Symbol::NewFolder:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M4 8.5 L9.1 8.5 L11.1 10.4 L20 10.4 L20 19 L4 19 Z"/><path d="M14 13 L14 17"/><path d="M12 15 L16 15"/></svg>)SVG";
    case Symbol::Applications:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.9" stroke-linecap="round" stroke-linejoin="round"><path d="M6 20 L12 4 L18 20"/><path d="M8.2 14.5 L15.8 14.5"/><path d="M4.5 20 L19.5 20"/></svg>)SVG";
    case Symbol::Desktop:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="16" height="12" rx="2.4"/><path d="M9 20 L15 20"/><path d="M7 21.5 L17 21.5"/></svg>)SVG";
    case Symbol::Download:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><circle cx="12" cy="12" r="8"/><path d="M12 7 L12 15"/><path d="M8.5 12 L12 15.5 L15.5 12"/></svg>)SVG";
    case Symbol::SortGroup:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.6" stroke-linecap="round" stroke-linejoin="round"><rect x="4" y="5" width="4.2" height="3" rx="0.8"/><rect x="10" y="5" width="4.2" height="3" rx="0.8"/><rect x="16" y="5" width="4.2" height="3" rx="0.8"/><rect x="4" y="11" width="4.2" height="3" rx="0.8"/><rect x="10" y="11" width="4.2" height="3" rx="0.8"/><rect x="16" y="11" width="4.2" height="3" rx="0.8"/><rect x="4" y="17" width="4.2" height="3" rx="0.8"/><rect x="10" y="17" width="4.2" height="3" rx="0.8"/><path d="M17 16 L19.2 18.2 L21.4 16"/></svg>)SVG";
    case Symbol::Duplicate:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><rect x="6" y="8" width="10" height="12" rx="2"/><rect x="10" y="4" width="10" height="12" rx="2"/></svg>)SVG";
    case Symbol::NewDocument:
        return R"SVG(<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.7" stroke-linecap="round" stroke-linejoin="round"><path d="M7 4 L14 4 L18 8 L18 20 L7 20 Z"/><path d="M14 4 L14 8 L18 8"/><path d="M12.5 12 L12.5 17"/><path d="M10 14.5 L15 14.5"/></svg>)SVG";
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

} // namespace phenotype::icons

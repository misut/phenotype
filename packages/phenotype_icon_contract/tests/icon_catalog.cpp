#include <cassert>
#include <cstdio>
#include <string_view>

import phenotype.icon_catalog;

namespace icons = phenotype::icon_catalog;

int main() {
    assert(icons::style_name() == "macos_rounded_outline_svg");
    assert(icons::source_format() == "svg");
    assert(icons::style_reference().find("Apple HIG") != std::string_view::npos);
    assert(icons::style_reference().find("macOS Finder") != std::string_view::npos);
    assert(icons::asset_policy().find("no Apple") != std::string_view::npos);
    assert(icons::svg_subset_policy() == "bounded_svg_icon_subset");
    assert(icons::svg_supported_path_commands().find("A Z")
           != std::string_view::npos);
    assert(icons::svg_arc_policy().find("bounded cubic Bezier")
           != std::string_view::npos);
    assert(icons::all_symbol_count == 31);
    assert(icons::sidebar_symbol_count == 11);
    assert(icons::toolbar_symbol_count == 15);
    assert(icons::outline_symbol_count == 30);
    assert(icons::filled_symbol_count == 1);
    assert(icons::hierarchical_symbol_count == 20);
    assert(icons::reference_symbol_count == icons::all_symbol_count);

    unsigned int outline_count = 0;
    unsigned int filled_count = 0;
    unsigned int hierarchical_count = 0;
    for (unsigned int i = 0; i < icons::all_symbol_count; ++i) {
        auto const symbol = icons::symbol_at(i);
        auto const desc = icons::descriptor(symbol);
        assert(desc.symbol == symbol);
        assert(!desc.name.empty());
        assert(!desc.semantic_reference_name.empty());
        assert(desc.style == icons::style_name());
        assert(desc.reference_family == icons::reference_family());
        assert(desc.reference_policy == icons::reference_policy());
        assert(desc.grid_size == 24.0f);
        assert(desc.phenotype_owned);
        assert(!desc.uses_sf_symbols_asset);
        if (desc.filled)
            ++filled_count;
        else
            ++outline_count;
        if (desc.supports_hierarchical_opacity)
            ++hierarchical_count;
    }
    assert(outline_count == icons::outline_symbol_count);
    assert(filled_count == icons::filled_symbol_count);
    assert(hierarchical_count == icons::hierarchical_symbol_count);

    assert(icons::semantic_reference_name(icons::Symbol::AirDrop) == "airdrop");
    assert(icons::sidebar_symbol_at(8) == icons::Symbol::AirDrop);
    assert(icons::toolbar_symbol_at(10) == icons::Symbol::Search);
    assert(icons::interaction_tone_policy() == "macos_finder_interaction_tones");
    assert(icons::macos_interaction_tone(
               icons::SymbolPresentationRole::Sidebar,
               icons::SymbolInteractionState{true, true})
           == icons::SymbolTone::Accent);
    assert(icons::macos_interaction_tone(
               icons::SymbolPresentationRole::Sidebar,
               icons::SymbolInteractionState{false, true})
           == icons::SymbolTone::Primary);
    assert(icons::macos_interaction_tone(
               icons::SymbolPresentationRole::Toolbar,
               icons::SymbolInteractionState{false, false})
           == icons::SymbolTone::Disabled);
    assert(icons::file_type_color_policy() == "macos_finder_file_type_tints");
    auto const folder_color = icons::macos_file_type_color(icons::Symbol::Folder);
    auto const image_color = icons::macos_file_type_color(icons::Symbol::Image);
    auto const document_color =
        icons::macos_file_type_color(icons::Symbol::Document);
    assert(folder_color.b > folder_color.r);
    assert(image_color.g > document_color.g);
    assert(document_color.r == document_color.g);
    assert(document_color.a == 255);
    assert(icons::symbol_presentation_role_name(
               icons::default_presentation_role(icons::Symbol::Recents))
           == "sidebar");
    assert(icons::symbol_scale_name(
               icons::default_scale(icons::SymbolPresentationRole::Sidebar))
           == "large");
    assert(icons::point_size(icons::SymbolScale::Medium) == 24.0f);

    std::puts("PASS: phenotype icon catalog contract is stable");
    return 0;
}

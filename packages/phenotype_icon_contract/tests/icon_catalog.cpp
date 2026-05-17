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
    assert(icons::svg_supported_style_attributes().find("stroke-linecap")
           != std::string_view::npos);
    assert(icons::svg_supported_style_attributes().find("stroke-linejoin")
           != std::string_view::npos);
    assert(icons::svg_arc_policy().find("isolated circular path A/a")
           != std::string_view::npos);
    assert(icons::svg_arc_policy().find("bounded cubic Bezier")
           != std::string_view::npos);
    assert(icons::stroke_geometry_policy() == "round_cap_round_join_svg_strokes");
    assert(icons::stroke_cap_policy() == "round");
    assert(icons::stroke_join_policy() == "round");
    assert(icons::interface_metaphor_policy()
           == "familiar_simplified_macos_symbol_metaphors");
    assert(icons::visual_consistency_policy()
           == "consistent_size_stroke_detail_and_perspective");
    assert(icons::toolbar_symbol_chrome_policy()
           == "borderless_toolbar_symbols_inside_grouped_controls");
    assert(icons::sidebar_symbol_color_policy()
           == "accent_selected_user_tint_compatible_sidebar_symbols");
    assert(icons::metrics_policy()
           == "macos_finder_role_metrics_with_explicit_hit_targets");
    assert(icons::hit_target_policy().find("sidebar=38pt")
           != std::string_view::npos);
    assert(icons::all_symbol_count == 31);
    assert(icons::sidebar_symbol_count == 11);
    assert(icons::toolbar_symbol_count == 15);
    assert(icons::outline_symbol_count == 30);
    assert(icons::filled_symbol_count == 1);
    assert(icons::hierarchical_symbol_count == 20);
    assert(icons::reference_symbol_count == icons::all_symbol_count);
    assert(icons::svg_path_arc_symbol_count == 1);
    assert(icons::round_stroke_symbol_count == icons::outline_symbol_count);

    unsigned int outline_count = 0;
    unsigned int filled_count = 0;
    unsigned int hierarchical_count = 0;
    unsigned int arc_path_count = 0;
    unsigned int round_stroke_count = 0;
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
        assert(icons::symbol_from_name(desc.name).has_value());
        assert(*icons::symbol_from_name(desc.name) == symbol);
        assert(icons::symbol_from_semantic_reference_name(
                   desc.semantic_reference_name)
                   .has_value());
        assert(*icons::symbol_from_semantic_reference_name(
                   desc.semantic_reference_name)
                == symbol);
        auto const metrics = icons::symbol_metrics(symbol);
        assert(metrics.role == icons::default_presentation_role(symbol));
        assert(metrics.grid_size == desc.grid_size);
        assert(metrics.point_size <= metrics.hit_target_size);
        assert(metrics.content_inset >= 0.0f);
        assert(metrics.stroke_width == desc.default_stroke_width);
        assert(desc.phenotype_owned);
        assert(!desc.uses_sf_symbols_asset);
        if (desc.filled)
            ++filled_count;
        else {
            ++outline_count;
            assert(desc.round_stroke);
            assert(desc.stroke_cap == icons::SymbolStrokeCap::Round);
            assert(desc.stroke_join == icons::SymbolStrokeJoin::Round);
            assert(icons::symbol_stroke_cap_name(desc.stroke_cap) == "round");
            assert(icons::symbol_stroke_join_name(desc.stroke_join) == "round");
        }
        if (desc.supports_hierarchical_opacity)
            ++hierarchical_count;
        if (icons::uses_svg_path_arcs(symbol))
            ++arc_path_count;
        if (desc.round_stroke)
            ++round_stroke_count;
    }
    assert(outline_count == icons::outline_symbol_count);
    assert(filled_count == icons::filled_symbol_count);
    assert(hierarchical_count == icons::hierarchical_symbol_count);
    assert(arc_path_count == icons::svg_path_arc_symbol_count);
    assert(round_stroke_count == icons::round_stroke_symbol_count);

    assert(icons::semantic_reference_name(icons::Symbol::AirDrop) == "airdrop");
    assert(icons::uses_svg_path_arcs(icons::Symbol::AirDrop));
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
    assert(icons::hit_target_size(icons::SymbolPresentationRole::Toolbar)
           == 36.0f);
    assert(icons::hit_target_size(icons::SymbolPresentationRole::Sidebar)
           == 38.0f);
    auto const sidebar_metrics =
        icons::metrics(icons::SymbolPresentationRole::Sidebar);
    assert(sidebar_metrics.point_size == 26.0f);
    assert(sidebar_metrics.hit_target_size == 38.0f);
    assert(sidebar_metrics.optical_y_offset == -0.5f);
    assert(!icons::symbol_from_name("missing").has_value());
    assert(!icons::symbol_from_semantic_reference_name("missing").has_value());
    assert(icons::descriptor(icons::Symbol::Folder).layer_count
           == icons::symbol_layer_count(icons::Symbol::Folder));

    std::puts("PASS: phenotype icon catalog contract is stable");
    return 0;
}

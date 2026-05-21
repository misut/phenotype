export module phenotype_cli.icons_checks;

import phenotype.icon_catalog;
import phenotype_cli.common;
import std;

namespace icon_catalog = phenotype::icon_catalog;

export namespace phenotype_cli::icons {

using namespace phenotype_cli::common;

auto icon_catalog_checks() -> std::vector<Check> {
    unsigned int outline_count = 0;
    unsigned int filled_count = 0;
    unsigned int hierarchical_count = 0;
    unsigned int monochrome_count = 0;
    unsigned int regular_weight_count = 0;
    unsigned int palette_count = 0;
    unsigned int multicolor_count = 0;
    unsigned int arc_path_count = 0;
    unsigned int round_stroke_count = 0;
    unsigned int phenotype_owned_count = 0;
    unsigned int permissive_source_count = 0;
    unsigned int lucide_source_count = 0;
    unsigned int apple_asset_count = 0;
    unsigned int platform_extracted_count = 0;
    unsigned int runtime_fetch_count = 0;
    bool semantic_references = true;
    bool round_stroke_contract = true;
    bool round_cap_join_contract = true;
    bool text_weight_aligned = true;
    bool rendering_capability_contract = true;
    bool name_lookup_roundtrips = true;
    bool reference_lookup_roundtrips = true;
    bool metric_contract = true;
    bool svg_source_contract = true;
    bool file_type_symbol_set_contract =
        icon_catalog::file_type_symbol_count == 11
        && icon_catalog::file_type_symbol_at(0) == icon_catalog::Symbol::Folder
        && icon_catalog::file_type_symbol_at(2)
            == icon_catalog::Symbol::PdfDocument
        && icon_catalog::file_type_symbol_at(6)
            == icon_catalog::Symbol::Archive
        && icon_catalog::file_type_symbol_at(10)
            == icon_catalog::Symbol::PresentationDocument;
    auto const toolbar_chrome = icon_catalog::macos_control_chrome(
        icon_catalog::SymbolPresentationRole::Toolbar,
        icon_catalog::SymbolInteractionState{false, true});
    auto const sidebar_selected_chrome = icon_catalog::macos_control_chrome(
        icon_catalog::SymbolPresentationRole::Sidebar,
        icon_catalog::SymbolInteractionState{true, true});
    auto const toolbar_pressed_recipe = icon_catalog::macos_state_recipe(
        icon_catalog::SymbolPresentationRole::Toolbar,
        icon_catalog::SymbolInteractionState{false, true},
        icon_catalog::SymbolInteractionPhase::Pressed);
    auto const selected_sidebar_pressed_recipe = icon_catalog::macos_state_recipe(
        icon_catalog::SymbolPresentationRole::Sidebar,
        icon_catalog::SymbolInteractionState{true, true},
        icon_catalog::SymbolInteractionPhase::Pressed);
    auto const disabled_recipe = icon_catalog::macos_state_recipe(
        icon_catalog::SymbolPresentationRole::Toolbar,
        icon_catalog::SymbolInteractionState{false, false},
        icon_catalog::SymbolInteractionPhase::Hovered);
    for (unsigned int i = 0; i < icon_catalog::all_symbol_count; ++i) {
        auto const symbol = icon_catalog::symbol_at(i);
        auto const desc = icon_catalog::descriptor(symbol);
        auto const metrics = icon_catalog::symbol_metrics(symbol);
        auto const source = icon_catalog::svg_source(symbol);
        auto const name_lookup = icon_catalog::symbol_from_name(desc.name);
        auto const reference_lookup =
            icon_catalog::symbol_from_semantic_reference_name(
                desc.semantic_reference_name);
        if (desc.filled)
            ++filled_count;
        else
            ++outline_count;
        if (desc.supports_hierarchical_opacity)
            ++hierarchical_count;
        if (desc.supports_monochrome)
            ++monochrome_count;
        if (desc.default_weight == icon_catalog::SymbolWeight::Regular)
            ++regular_weight_count;
        if (desc.supports_palette)
            ++palette_count;
        if (desc.supports_multicolor)
            ++multicolor_count;
        if (icon_catalog::uses_svg_path_arcs(symbol))
            ++arc_path_count;
        if (desc.round_stroke)
            ++round_stroke_count;
        if (desc.phenotype_owned)
            ++phenotype_owned_count;
        if (icon_catalog::uses_lucide_source(symbol)) {
            ++permissive_source_count;
            ++lucide_source_count;
        }
        auto const attribution = icon_catalog::source_attribution(symbol);
        if (desc.uses_sf_symbols_asset || attribution.apple_asset)
            ++apple_asset_count;
        if (attribution.platform_extracted)
            ++platform_extracted_count;
        if (attribution.runtime_fetch_required)
            ++runtime_fetch_count;
        semantic_references =
            semantic_references && !desc.semantic_reference_name.empty();
        name_lookup_roundtrips =
            name_lookup_roundtrips && name_lookup.has_value()
            && *name_lookup == symbol;
        reference_lookup_roundtrips =
            reference_lookup_roundtrips && reference_lookup.has_value()
            && *reference_lookup == symbol;
        metric_contract =
            metric_contract
            && metrics.role == icon_catalog::default_presentation_role(symbol)
            && metrics.grid_size == desc.grid_size
            && metrics.point_size <= metrics.hit_target_size
            && metrics.content_inset >= 0.0f
            && metrics.stroke_width == desc.default_stroke_width;
        svg_source_contract =
            svg_source_contract
            && source.find("<svg") == 0
            && source.find("viewBox=\"0 0 24 24\"")
                != std::string_view::npos
            && source.find("currentColor") != std::string_view::npos;
        round_stroke_contract =
            round_stroke_contract && (desc.filled || desc.round_stroke);
        round_cap_join_contract =
            round_cap_join_contract
            && (desc.filled
                || (desc.stroke_cap == icon_catalog::SymbolStrokeCap::Round
                    && desc.stroke_join
                        == icon_catalog::SymbolStrokeJoin::Round));
        text_weight_aligned =
            text_weight_aligned && desc.text_weight_aligned;
        auto const capabilities = icon_catalog::rendering_capabilities(symbol);
        rendering_capability_contract =
            rendering_capability_contract
            && capabilities.policy
                == icon_catalog::rendering_capability_policy()
            && capabilities.monochrome == desc.supports_monochrome
            && capabilities.hierarchical
                == desc.supports_hierarchical_opacity
            && capabilities.palette == desc.supports_palette
            && capabilities.multicolor == desc.supports_multicolor;
    }

    return {
        {.name = "symbol_counts",
         .ok = outline_count == icon_catalog::outline_symbol_count
            && filled_count == icon_catalog::filled_symbol_count
            && hierarchical_count == icon_catalog::hierarchical_symbol_count,
         .detail = std::format(
             "all={} outline={} filled={} hierarchical={}",
             icon_catalog::all_symbol_count,
             outline_count,
             filled_count,
             hierarchical_count),
         .hint =
             "Update phenotype.icon_catalog counts when adding or removing built-in symbols."},
        {.name = "macos_reference_policy",
         .ok = icon_catalog::style_reference().find("macOS Finder")
            != std::string_view::npos
            && icon_catalog::reference_family()
                == std::string_view{"SF Symbols semantic reference"}
            && icon_catalog::interface_metaphor_policy()
                == std::string_view{
                    "familiar_simplified_macos_symbol_metaphors"}
            && icon_catalog::visual_consistency_policy()
                == std::string_view{
                    "consistent_size_stroke_detail_and_perspective"},
         .detail = std::string{icon_catalog::style_reference()},
         .hint =
             "Keep the icon catalog anchored to Apple HIG, macOS Finder, and SF Symbols semantic names."},
        {.name = "asset_ownership",
         .ok = phenotype_owned_count
                == icon_catalog::phenotype_owned_symbol_count
            && permissive_source_count
                == icon_catalog::permissive_source_symbol_count
            && lucide_source_count == icon_catalog::lucide_source_symbol_count
            && icon_catalog::lucide_unique_source_icon_count == 38
            && icon_catalog::lucide_source_icon_name_at(28)
                == std::string_view{"file-text"}
            && apple_asset_count == icon_catalog::apple_asset_symbol_count
            && platform_extracted_count
                == icon_catalog::platform_extracted_symbol_count
            && runtime_fetch_count
                == icon_catalog::runtime_fetched_symbol_count
            && icon_catalog::source_license_policy().find("ISC")
                != std::string_view::npos
            && icon_catalog::apple_asset_boundary().find("do not extract")
                != std::string_view::npos,
         .detail = std::format(
             "phenotype_owned={} permissive={} lucide={} lucide_unique={} apple_asset={} platform_extracted={} runtime_fetch={}",
             phenotype_owned_count,
             permissive_source_count,
             lucide_source_count,
             icon_catalog::lucide_unique_source_icon_count,
             apple_asset_count,
             platform_extracted_count,
             runtime_fetch_count),
         .hint =
             "Use only audited permissive SVG sources, and never embed Apple or SF Symbols vector artwork."},
        {.name = "source_attribution",
         .ok = icon_catalog::source_attribution_policy().find(
                   "pinned direct raw SVG URL")
                != std::string_view::npos
            && icon_catalog::source_attribution_policy().find(
                   "platform extraction flag")
                != std::string_view::npos
            && icon_catalog::source_acquisition_policy().find(
                   "runtime uses embedded SVG strings")
                != std::string_view::npos
            && icon_catalog::source_acquisition_policy().find(
                   "platform icon extraction disabled")
                != std::string_view::npos
            && icon_catalog::document_cache_policy().find(
                   "no_frame_parse_churn")
                != std::string_view::npos
            && icon_catalog::source_attribution(icon_catalog::Symbol::Folder)
                   .family
                == std::string_view{"Lucide"}
            && icon_catalog::source_attribution(icon_catalog::Symbol::Document)
                   .license
                == std::string_view{"ISC"}
            && icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                   .license
                == std::string_view{"MIT"}
            && icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                   .family
                == std::string_view{"Lucide"}
            && icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                   .icon_name
                == std::string_view{"search"}
            && icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                   .source_url
                   .find("raw.githubusercontent.com")
                != std::string_view::npos
            && icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                   .source_url
                   .find(icon_catalog::lucide_source_revision())
                != std::string_view::npos
            && icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                   .source_url
                   .find("/blob/")
                == std::string_view::npos
            && icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                   .source_url
                   .find("/main/")
                == std::string_view::npos
            && icon_catalog::source_attribution(icon_catalog::Symbol::Shared)
                   .family
                == std::string_view{"Lucide"}
            && icon_catalog::source_attribution(icon_catalog::Symbol::Shared)
                   .icon_name
                == std::string_view{"folder-symlink"}
            && !icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                    .platform_extracted
            && !icon_catalog::source_attribution(icon_catalog::Symbol::Search)
                    .runtime_fetch_required,
         .detail = std::string{icon_catalog::source_attribution_policy()},
         .hint =
             "Every embedded icon source must carry machine-readable family, icon name, exact license, pinned direct raw SVG URL, and source revision metadata."},
        {.name = "reference_sources",
         .ok = icon_catalog::reference_source_count == 8
            && icon_catalog::reference_source_at(0).apple_owned_artwork
            && !icon_catalog::reference_source_at(0)
                    .used_as_embedded_asset_source
            && !icon_catalog::reference_source_at(0).may_embed_svg_source
            && !icon_catalog::reference_source_at(0).runtime_fetch_allowed
            && !icon_catalog::reference_source_at(0)
                    .platform_extraction_allowed
            && icon_catalog::reference_source_at(2).url.find("SVG2/paths")
                != std::string_view::npos
            && icon_catalog::reference_source_at(3)
                    .used_as_embedded_asset_source
            && icon_catalog::reference_source_at(3).may_embed_svg_source
            && icon_catalog::reference_source_at(3).requires_notice
            && icon_catalog::reference_source_at(3)
                   .source_acquisition.find("pinned_raw_svg")
                != std::string_view::npos
            && icon_catalog::reference_source_at(3).license_policy.find("ISC")
                != std::string_view::npos
            && icon_catalog::reference_source_at(3)
                   .license_url.find("lucide.dev/license")
                != std::string_view::npos
            && icon_catalog::reference_source_at(4)
                    .license_policy.find("MIT")
                != std::string_view::npos
            && !icon_catalog::reference_source_at(4)
                    .used_as_embedded_asset_source
            && icon_catalog::reference_source_at(4).requires_notice
            && icon_catalog::reference_source_at(5)
                    .license_policy.find("Apache-2.0")
                != std::string_view::npos
            && icon_catalog::reference_source_at(5).may_embed_svg_source
            && icon_catalog::reference_source_at(5).requires_notice
            && icon_catalog::reference_source_at(6)
                    .license_policy.find("MIT")
                != std::string_view::npos
            && icon_catalog::reference_source_at(6).may_embed_svg_source
            && icon_catalog::reference_source_at(6).requires_notice
            && icon_catalog::reference_source_at(7)
                    .license_policy.find("MIT")
                != std::string_view::npos
            && icon_catalog::reference_source_at(7).may_embed_svg_source
            && icon_catalog::reference_source_at(7).requires_notice,
         .detail = std::format(
             "references={} embedded_source={} license_url={} apple_reference={}",
             icon_catalog::reference_source_count,
             icon_catalog::reference_source_at(3).name,
             icon_catalog::reference_source_at(3).license_url,
             icon_catalog::reference_source_at(0).name),
         .hint =
             "Keep icon reference sources explicit so provenance, license URL, notice requirement, and no-Apple-artwork boundaries are debuggable."},
        {.name = "svg_source_contract",
         .ok = svg_source_contract
            && icon_catalog::svg_source(icon_catalog::Symbol::Applications)
                   .find("stroke-linecap=\"round\"")
                != std::string_view::npos,
         .detail = "all built-in symbols expose audited SVG source",
         .hint =
             "Keep icon SVG source in phenotype.icon_catalog so CLI diagnostics can inspect icons without platform renderer dependencies."},
        {.name = "semantic_references",
         .ok = semantic_references
            && icon_catalog::semantic_reference_name(
                   icon_catalog::Symbol::AirDrop)
                == std::string_view{"airdrop"},
         .detail = std::format(
             "reference_count={} airdrop={}",
             icon_catalog::reference_symbol_count,
             icon_catalog::semantic_reference_name(
                 icon_catalog::Symbol::AirDrop)),
         .hint =
             "Every phenotype-owned glyph needs a stable semantic SF Symbols reference name for debug traces."},
        {.name = "lookup_and_metrics",
         .ok = name_lookup_roundtrips
            && reference_lookup_roundtrips
            && metric_contract
            && icon_catalog::metrics_policy()
                == std::string_view{
                    "macos_finder_role_metrics_with_explicit_hit_targets"}
            && icon_catalog::hit_target_size(
                   icon_catalog::SymbolPresentationRole::Toolbar)
                == 36.0f
            && icon_catalog::hit_target_size(
                   icon_catalog::SymbolPresentationRole::Sidebar)
                == 38.0f,
         .detail = std::format(
             "{}; {}",
             icon_catalog::metrics_policy(),
             icon_catalog::hit_target_policy()),
         .hint =
             "Keep symbol lookup and macOS role metrics pure so apps can map string commands to Finder-like glyph presentation without platform APIs."},
        {.name = "file_type_symbol_set",
         .ok = file_type_symbol_set_contract,
         .detail = std::format(
             "file_type_count={} first={} pdf={} last={}",
             icon_catalog::file_type_symbol_count,
             icon_catalog::name(icon_catalog::file_type_symbol_at(0)),
             icon_catalog::name(icon_catalog::file_type_symbol_at(2)),
             icon_catalog::name(icon_catalog::file_type_symbol_at(10))),
         .hint =
             "Keep Finder-like file-type glyphs exposed as a pure catalog set for examples, CLI, and artifact contracts."},
        {.name = "stroke_and_weight",
         .ok = round_stroke_contract
            && round_cap_join_contract
            && text_weight_aligned
            && regular_weight_count == icon_catalog::regular_weight_symbol_count
            && round_stroke_count == icon_catalog::round_stroke_symbol_count
            && icon_catalog::stroke_geometry_policy()
                == std::string_view{"round_cap_round_join_svg_strokes"}
            && icon_catalog::default_weight_policy()
                == std::string_view{"regular_text_weight_aligned"},
         .detail = std::format(
             "round_stroke={} round_cap_join={} text_weight_aligned={} regular_weight={} count={}",
                               round_stroke_contract ? "true" : "false",
                               round_cap_join_contract ? "true" : "false",
                               text_weight_aligned ? "true" : "false",
                               regular_weight_count,
                               round_stroke_count),
        .hint =
            "Mac-like icons should stay round-cap, round-join, and text-weight aligned."},
        {.name = "rendering_capabilities",
         .ok = rendering_capability_contract
            && monochrome_count == icon_catalog::monochrome_symbol_count
            && hierarchical_count == icon_catalog::hierarchical_symbol_count
            && palette_count == icon_catalog::palette_symbol_count
            && multicolor_count == icon_catalog::multicolor_symbol_count
            && icon_catalog::rendering_capability_policy().find(
                   "sf_symbols_mode_names")
                != std::string_view::npos,
         .detail = std::format(
             "monochrome={} hierarchical={} palette={} multicolor={}",
             monochrome_count,
             hierarchical_count,
             palette_count,
             multicolor_count),
         .hint =
             "Expose SF Symbols rendering-mode names explicitly while keeping unsupported palette/multicolor output deterministic."},
        {.name = "svg_path_subset",
         .ok = icon_catalog::svg_subset_policy()
                == std::string_view{"bounded_svg_icon_subset"}
            && icon_catalog::svg_supported_path_commands().find("A Z")
                != std::string_view::npos
            && icon_catalog::svg_supported_style_attributes()
                   .find("stroke-linecap")
                != std::string_view::npos
            && icon_catalog::svg_supported_style_attributes()
                   .find("stroke-linejoin")
                != std::string_view::npos
            && icon_catalog::svg_arc_policy().find("isolated circular path A/a")
                != std::string_view::npos
            && icon_catalog::svg_arc_policy().find("bounded cubic Bezier")
                != std::string_view::npos
            && arc_path_count == icon_catalog::svg_path_arc_symbol_count,
         .detail = std::format(
             "{}; arc_path_symbols={}",
             icon_catalog::svg_supported_path_commands(),
             arc_path_count),
         .hint =
             "Keep the built-in icon SVG subset broad enough for macOS-style rounded glyph geometry."},
        {.name = "interaction_tone_policy",
         .ok = icon_catalog::interaction_tone_policy()
                == std::string_view{"macos_finder_interaction_tones"}
            && icon_catalog::toolbar_symbol_chrome_policy()
                == std::string_view{
                    "borderless_toolbar_symbols_inside_grouped_controls"}
            && icon_catalog::sidebar_symbol_color_policy()
                == std::string_view{
                    "accent_selected_user_tint_compatible_sidebar_symbols"}
            && icon_catalog::symbol_control_chrome_policy()
                == std::string_view{"macos_finder_symbol_state_chrome"}
            && toolbar_chrome.hover_background_color.a == 120
            && sidebar_selected_chrome.background_color.a == 150
            && icon_catalog::macos_interaction_tone(
                   icon_catalog::SymbolPresentationRole::Sidebar,
                   icon_catalog::SymbolInteractionState{true, true})
                == icon_catalog::SymbolTone::Accent
            && icon_catalog::macos_interaction_tone(
                   icon_catalog::SymbolPresentationRole::Toolbar,
                   icon_catalog::SymbolInteractionState{false, false})
                == icon_catalog::SymbolTone::Disabled,
         .detail = std::format(
             "{}; {}",
             icon_catalog::interaction_tone_policy(),
             icon_catalog::symbol_control_chrome_policy()),
         .hint =
             "Keep selected, hover, and disabled icon chrome in the pure catalog contract."},
        {.name = "interaction_phase_policy",
         .ok = icon_catalog::symbol_interaction_phase_policy()
                == std::string_view{
                    "macos_finder_normal_hover_pressed_symbol_chrome"}
            && icon_catalog::symbol_interaction_phase_count == 3
            && icon_catalog::symbol_interaction_phase_name(
                   icon_catalog::SymbolInteractionPhase::Pressed)
                == std::string_view{"pressed"}
            && toolbar_pressed_recipe.background_color.a == 150
            && toolbar_pressed_recipe.symbol_opacity < 1.0f
            && toolbar_pressed_recipe.scale < 1.0f
            && selected_sidebar_pressed_recipe.background_color.a == 196
            && disabled_recipe.symbol_opacity < 0.5f,
         .detail = std::format(
             "{}; phases={}",
             icon_catalog::symbol_interaction_phase_policy(),
             icon_catalog::symbol_interaction_phase_count),
         .hint =
             "Keep normal, hovered, pressed, selected, and disabled icon state recipes in the pure catalog contract."},
        {.name = "file_type_palette",
         .ok = icon_catalog::file_type_color_policy()
                == std::string_view{"macos_finder_file_type_tints"}
            && icon_catalog::macos_file_type_color(icon_catalog::Symbol::Folder).b
                > icon_catalog::macos_file_type_color(icon_catalog::Symbol::Folder).r,
         .detail = std::string{icon_catalog::file_type_color_policy()},
         .hint =
             "Keep Finder-style file type icon tints exposed as pure catalog data."},
    };
}

}

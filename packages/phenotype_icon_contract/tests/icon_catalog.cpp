#include <cassert>
#include <cstdio>
#include <string>
#include <string_view>

import phenotype.icon_catalog;

namespace icons = phenotype::icon_catalog;

int main() {
    assert(icons::style_name() == "google_material_symbols_outlined_svg");
    assert(icons::source_format() == "svg");
    assert(icons::style_reference().find("Apple HIG") != std::string_view::npos);
    assert(icons::style_reference().find("macOS Finder") != std::string_view::npos);
    assert(icons::asset_policy().find("no Apple") != std::string_view::npos);
    assert(icons::asset_policy().find("Google Material Symbols")
           != std::string_view::npos);
    assert(icons::source_license_policy().find("Apache-2.0")
           != std::string_view::npos);
    assert(icons::source_license_policy().find("bundled license text")
           != std::string_view::npos);
    assert(icons::preferred_external_source_policy().find(
               "Google Material Symbols")
           != std::string_view::npos);
    assert(icons::source_acquisition_policy().find(
               "pinned Google Material Symbols")
           != std::string_view::npos);
    assert(icons::source_acquisition_policy().find("runtime uses embedded SVG strings")
           != std::string_view::npos);
    assert(icons::source_acquisition_policy().find("platform icon extraction disabled")
           != std::string_view::npos);
    assert(icons::source_acquisition_policy().find("redistribution rights are explicit")
           != std::string_view::npos);
    assert(icons::document_cache_policy().find(
               "keyed_by_symbol_descriptor")
           != std::string_view::npos);
    assert(icons::document_cache_policy().find("no_frame_parse_churn")
           != std::string_view::npos);
    assert(icons::material_symbols_source_revision().size() == 40);
    assert(icons::material_symbols_style_count == 3);
    assert(icons::material_symbols_source_variant_count
           == icons::material_symbols_unique_source_icon_count
                  * icons::material_symbols_style_count);
    assert(icons::default_material_symbols_style()
           == icons::MaterialSymbolsStyle::Outlined);
    assert(icons::style_name(icons::MaterialSymbolsStyle::Outlined)
           == "google_material_symbols_outlined_svg");
    assert(icons::style_name(icons::MaterialSymbolsStyle::Rounded)
           == "google_material_symbols_rounded_svg");
    assert(icons::style_name(icons::MaterialSymbolsStyle::Sharp)
           == "google_material_symbols_sharp_svg");
    assert(icons::material_symbols_style_from_name("outlined").has_value());
    assert(*icons::material_symbols_style_from_name("rounded")
           == icons::MaterialSymbolsStyle::Rounded);
    assert(icons::material_symbols_font_family(
               icons::MaterialSymbolsStyle::Sharp)
           == "Material Symbols Sharp");
    assert(icons::source_attribution_policy().find("pinned direct raw SVG URL")
           != std::string_view::npos);
    assert(icons::source_attribution_policy().find("source revision")
           != std::string_view::npos);
    assert(icons::source_attribution_policy().find("platform extraction flag")
           != std::string_view::npos);
    assert(icons::source_attribution_policy().find("runtime fetch flag")
           != std::string_view::npos);
    assert(icons::apple_asset_boundary().find("design references only")
           != std::string_view::npos);
    assert(icons::apple_asset_boundary().find("do not extract or embed")
           != std::string_view::npos);
    assert(icons::apple_asset_boundary().find("redistribution clearance")
           != std::string_view::npos);
    assert(icons::reference_source_count == 6);
    auto const apple_icons = icons::reference_source_at(0);
    assert(apple_icons.name.find("Apple") != std::string_view::npos);
    assert(apple_icons.url.find("developer.apple.com")
           != std::string_view::npos);
    assert(!apple_icons.used_as_embedded_asset_source);
    assert(apple_icons.apple_owned_artwork);
    assert(!apple_icons.may_embed_svg_source);
    assert(!apple_icons.runtime_fetch_allowed);
    assert(!apple_icons.platform_extraction_allowed);
    assert(apple_icons.license_url.find("apple.com/legal")
           != std::string_view::npos);
    assert(apple_icons.source_acquisition
           == std::string_view{"reference_only_no_embedding"});
    auto const svg_paths = icons::reference_source_at(2);
    assert(svg_paths.name.find("W3C SVG") != std::string_view::npos);
    assert(svg_paths.url.find("SVG2/paths")
           != std::string_view::npos);
    assert(!svg_paths.apple_owned_artwork);
    assert(!svg_paths.may_embed_svg_source);
    auto const material_symbols_reference = icons::reference_source_at(3);
    assert(material_symbols_reference.name == std::string_view{"Google Material Symbols"});
    assert(material_symbols_reference.license_policy.find("Apache-2.0")
           != std::string_view::npos);
    assert(material_symbols_reference.used_as_embedded_asset_source);
    assert(!material_symbols_reference.apple_owned_artwork);
    assert(material_symbols_reference.may_embed_svg_source);
    assert(material_symbols_reference.requires_notice);
    assert(!material_symbols_reference.runtime_fetch_allowed);
    assert(!material_symbols_reference.platform_extraction_allowed);
    assert(material_symbols_reference.license_url.find("google/material-design-icons")
           != std::string_view::npos);
    assert(material_symbols_reference.source_acquisition.find("pinned_raw_svg")
           != std::string_view::npos);
    auto const tabler_reference = icons::reference_source_at(4);
    assert(tabler_reference.name.find("Tabler") != std::string_view::npos);
    assert(tabler_reference.url.find("tabler/tabler-icons")
           != std::string_view::npos);
    assert(tabler_reference.license_policy.find("MIT")
           != std::string_view::npos);
    assert(!tabler_reference.used_as_embedded_asset_source);
    assert(tabler_reference.license_url.find("tabler-icons")
           != std::string_view::npos);
    assert(tabler_reference.may_embed_svg_source);
    assert(tabler_reference.requires_notice);
    auto const iconoir_reference = icons::reference_source_at(5);
    assert(iconoir_reference.name.find("Iconoir") != std::string_view::npos);
    assert(iconoir_reference.url.find("iconoir-icons/iconoir")
           != std::string_view::npos);
    assert(iconoir_reference.license_policy.find("MIT")
           != std::string_view::npos);
    assert(!iconoir_reference.used_as_embedded_asset_source);
    assert(iconoir_reference.license_url.find("iconoir-icons/iconoir")
           != std::string_view::npos);
    assert(iconoir_reference.may_embed_svg_source);
    assert(iconoir_reference.requires_notice);
    assert(icons::svg_subset_policy() == "bounded_material_symbols_svg_subset");
    assert(icons::svg_supported_path_commands().find("C S Z")
           != std::string_view::npos);
    assert(icons::svg_supported_style_attributes().find("stroke-linecap")
           != std::string_view::npos);
    assert(icons::svg_supported_style_attributes().find("stroke-linejoin")
           != std::string_view::npos);
    assert(icons::svg_arc_policy().find("avoid SVG arc commands")
           != std::string_view::npos);
    assert(icons::stroke_geometry_policy() == "material_symbols_filled_path_geometry");
    assert(icons::stroke_cap_policy() == "not_applicable");
    assert(icons::stroke_join_policy() == "not_applicable");
    assert(icons::interface_metaphor_policy()
           == "familiar_simplified_macos_symbol_metaphors");
    assert(icons::visual_consistency_policy()
           == "consistent_material_symbols_new_style_selectable_size_detail_and_perspective");
    assert(icons::toolbar_symbol_chrome_policy()
           == "borderless_toolbar_symbols_inside_grouped_controls");
    assert(icons::sidebar_symbol_color_policy()
           == "accent_selected_user_tint_compatible_sidebar_symbols");
    assert(icons::symbol_control_chrome_policy()
           == "macos_finder_symbol_state_chrome");
    assert(icons::symbol_interaction_phase_policy()
           == "macos_finder_normal_hover_pressed_symbol_chrome");
    assert(icons::symbol_interaction_phase_count == 3);
    assert(icons::symbol_interaction_phase_name(
               icons::SymbolInteractionPhase::Normal)
           == "normal");
    assert(icons::symbol_interaction_phase_name(
               icons::SymbolInteractionPhase::Hovered)
           == "hovered");
    assert(icons::symbol_interaction_phase_name(
               icons::SymbolInteractionPhase::Pressed)
           == "pressed");
    assert(icons::default_weight_policy()
           == "regular_material_symbols_weight_aligned");
    assert(icons::rendering_capability_policy().find("material_symbols_monochrome")
           != std::string_view::npos);
    assert(icons::metrics_policy()
           == "macos_finder_role_metrics_with_explicit_hit_targets");
    assert(icons::hit_target_policy().find("sidebar=38pt")
           != std::string_view::npos);
    assert(icons::hit_target_policy().find("activation minimum=44pt")
           != std::string_view::npos);
    assert(icons::all_symbol_count == 39);
    assert(icons::phenotype_owned_symbol_count == 0);
    assert(icons::permissive_source_symbol_count == icons::all_symbol_count);
    assert(icons::material_symbols_source_symbol_count == icons::all_symbol_count);
    assert(icons::material_symbols_unique_source_icon_count == 39);
    assert(icons::apple_asset_symbol_count == 0);
    assert(icons::platform_extracted_symbol_count == 0);
    assert(icons::runtime_fetched_symbol_count == 0);
    assert(icons::audited_symbol_source_count == icons::all_symbol_count);
    assert(icons::sidebar_symbol_count == 11);
    assert(icons::toolbar_symbol_count == 15);
    assert(icons::file_type_symbol_count == 11);
    assert(icons::outline_symbol_count == icons::all_symbol_count);
    assert(icons::filled_symbol_count == icons::all_symbol_count);
    assert(icons::hierarchical_symbol_count == 0);
    assert(icons::monochrome_symbol_count == icons::all_symbol_count);
    assert(icons::regular_weight_symbol_count == icons::all_symbol_count);
    assert(icons::palette_symbol_count == 0);
    assert(icons::multicolor_symbol_count == 0);
    assert(icons::reference_symbol_count == icons::all_symbol_count);
    assert(icons::svg_path_arc_symbol_count == 0);
    assert(icons::round_stroke_symbol_count == 0);
    assert(icons::material_symbols_source_icon_name_at(0) == "chevron_left");
    assert(icons::material_symbols_source_icon_name_at(32) == "picture_as_pdf");
    assert(icons::material_symbols_source_icon_name_at(
               icons::material_symbols_unique_source_icon_count - 1)
           == "slideshow");
    unsigned int unique_source_count = 0;
    for (unsigned int i = 0; i < icons::material_symbols_unique_source_icon_count; ++i) {
        auto const icon_name = icons::material_symbols_source_icon_name_at(i);
        assert(!icon_name.empty());
        assert(icons::material_symbols_source_url(icon_name).find(
                   icons::material_symbols_source_revision())
               != std::string_view::npos);
        assert(icons::material_symbols_source_url(icon_name).find(
                   "raw.githubusercontent.com")
               != std::string_view::npos);
        for (unsigned int style_index = 0;
             style_index < icons::material_symbols_style_count;
             ++style_index) {
            auto const style = icons::material_symbols_style_at(style_index);
            auto const source_url =
                icons::material_symbols_source_url(icon_name, style);
            assert(source_url.find(
                       icons::material_symbols_source_directory(style))
                   != std::string_view::npos);
            assert(source_url.find("raw.githubusercontent.com")
                   != std::string_view::npos);
        }
        for (unsigned int j = 0; j < i; ++j) {
            assert(icons::material_symbols_source_icon_name_at(j) != icon_name);
        }
        ++unique_source_count;
    }
    assert(unique_source_count == icons::material_symbols_unique_source_icon_count);

    unsigned int outline_count = 0;
    unsigned int filled_count = 0;
    unsigned int hierarchical_count = 0;
    unsigned int arc_path_count = 0;
    unsigned int round_stroke_count = 0;
    unsigned int phenotype_owned_count = 0;
    unsigned int material_symbols_count = 0;
    unsigned int apple_asset_count = 0;
    unsigned int platform_extracted_count = 0;
    unsigned int runtime_fetch_count = 0;
    for (unsigned int i = 0; i < icons::all_symbol_count; ++i) {
        auto const symbol = icons::symbol_at(i);
        auto const desc = icons::descriptor(symbol);
        auto const source = icons::svg_source(symbol);
        auto const attribution = icons::source_attribution(symbol);
        assert(desc.symbol == symbol);
        assert(!desc.name.empty());
        assert(!desc.semantic_reference_name.empty());
        assert(source.find("<svg") == 0);
        assert(source.find("viewBox=\"0 -960 960 960\"") != std::string_view::npos);
        assert(source.find("currentColor") != std::string_view::npos);
        assert(desc.style == icons::style_name());
        assert(desc.reference_family == icons::reference_family());
        assert(desc.reference_policy == icons::reference_policy());
        assert(desc.grid_size == 24.0f);
        assert(desc.default_weight == icons::SymbolWeight::Regular);
        assert(icons::symbol_weight_name(desc.default_weight) == "regular");
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
        if (desc.phenotype_owned)
            ++phenotype_owned_count;
        assert(!desc.uses_sf_symbols_asset);
        if (icons::uses_material_symbols_source(symbol)) {
            ++material_symbols_count;
            assert(!desc.phenotype_owned);
            assert(attribution.family == "Google Material Symbols");
            assert(attribution.style == "outlined");
            assert(attribution.license == icons::material_symbols_icon_license(
                       attribution.icon_name));
            assert(attribution.source_url.find("google/material-design-icons")
                   != std::string_view::npos);
            assert(attribution.source_url.find("raw.githubusercontent.com")
                   != std::string_view::npos);
            assert(attribution.source_url.find(icons::material_symbols_source_revision())
                   != std::string_view::npos);
            assert(attribution.source_url.find("/blob/")
                   == std::string_view::npos);
            assert(attribution.source_url.find("/main/")
                   == std::string_view::npos);
            assert(attribution.license_url == icons::material_symbols_license_url());
            assert(attribution.source_revision
                   == icons::material_symbols_source_revision());
            assert(attribution.modified_for_phenotype);
        } else {
            assert(desc.phenotype_owned);
            assert(attribution.family == "phenotype");
            assert(attribution.license == "MIT");
            assert(attribution.source_revision == "phenotype-repository");
        }
        assert(!attribution.apple_asset);
        assert(!attribution.platform_extracted);
        assert(!attribution.runtime_fetch_required);
        if (attribution.apple_asset)
            ++apple_asset_count;
        if (attribution.platform_extracted)
            ++platform_extracted_count;
        if (attribution.runtime_fetch_required)
            ++runtime_fetch_count;
        auto const capabilities = icons::rendering_capabilities(symbol);
        assert(capabilities.monochrome);
        assert(capabilities.hierarchical == desc.supports_hierarchical_opacity);
        assert(!capabilities.palette);
        assert(!capabilities.multicolor);
        assert(capabilities.policy == icons::rendering_capability_policy());
        assert(desc.supports_monochrome);
        assert(!desc.supports_palette);
        assert(!desc.supports_multicolor);
        if (desc.variant == icons::SymbolVariant::Outline)
            ++outline_count;
        if (desc.filled)
            ++filled_count;
        assert(desc.filled);
        assert(!desc.round_stroke);
        assert(desc.default_stroke_width == 0.0f);
        assert(desc.preferred_rendering == icons::SymbolRenderingMode::Monochrome);
        if (desc.supports_hierarchical_opacity)
            ++hierarchical_count;
        if (icons::uses_svg_path_arcs(symbol))
            ++arc_path_count;
        if (desc.round_stroke)
            ++round_stroke_count;
    }
    auto const rounded_search = icons::svg_source(
        icons::Symbol::Search,
        icons::MaterialSymbolsStyle::Rounded);
    auto const sharp_search = icons::svg_source(
        icons::Symbol::Search,
        icons::MaterialSymbolsStyle::Sharp);
    assert(rounded_search.find("currentColor") != std::string_view::npos);
    assert(sharp_search.find("currentColor") != std::string_view::npos);
    assert(icons::descriptor(
               icons::Symbol::Search,
               icons::MaterialSymbolsStyle::Rounded)
               .style
           == "google_material_symbols_rounded_svg");
    assert(icons::source_attribution(
               icons::Symbol::Search,
               icons::MaterialSymbolsStyle::Sharp)
               .style
           == "sharp");
    assert(outline_count == icons::outline_symbol_count);
    assert(filled_count == icons::filled_symbol_count);
    assert(hierarchical_count == icons::hierarchical_symbol_count);
    assert(arc_path_count == icons::svg_path_arc_symbol_count);
    assert(round_stroke_count == icons::round_stroke_symbol_count);
    assert(phenotype_owned_count == icons::phenotype_owned_symbol_count);
    assert(material_symbols_count == icons::material_symbols_source_symbol_count);
    assert(apple_asset_count == icons::apple_asset_symbol_count);
    assert(platform_extracted_count == icons::platform_extracted_symbol_count);
    assert(runtime_fetch_count == icons::runtime_fetched_symbol_count);

    assert(icons::semantic_reference_name(icons::Symbol::AirDrop) == "airdrop");
    assert(icons::semantic_reference_name(icons::Symbol::ChevronUp)
           == "chevron.up");
    assert(icons::semantic_reference_name(icons::Symbol::PdfDocument)
           == "doc.richtext");
    assert(icons::semantic_reference_name(icons::Symbol::TextDocument)
           == "doc.plaintext");
    assert(icons::semantic_reference_name(icons::Symbol::Archive)
           == "archivebox");
    assert(!icons::uses_svg_path_arcs(icons::Symbol::AirDrop));
    assert(icons::uses_material_symbols_source(icons::Symbol::Folder));
    assert(icons::uses_material_symbols_source(icons::Symbol::More));
    assert(icons::uses_material_symbols_source(icons::Symbol::AirDrop));
    assert(icons::uses_material_symbols_source(icons::Symbol::Shared));
    assert(icons::uses_material_symbols_source(icons::Symbol::SortGroup));
    assert(icons::source_attribution(icons::Symbol::More).icon_name
           == "more_horiz");
    assert(icons::source_attribution(icons::Symbol::AirDrop).icon_name
           == "nearby");
    assert(icons::source_attribution(icons::Symbol::Shared).icon_name
           == "folder_shared");
    assert(icons::source_attribution(icons::Symbol::SortGroup).icon_name
           == "swap_vert");
    assert(icons::source_attribution(icons::Symbol::Folder).icon_name
           == "folder");
    assert(icons::source_attribution(icons::Symbol::Movie).icon_name
           == "movie");
    assert(icons::source_attribution(icons::Symbol::AudioDocument).icon_name
           == "audio_file");
    assert(icons::source_attribution(icons::Symbol::CodeDocument).icon_name
           == "code");
    assert(icons::source_attribution(
               icons::Symbol::SpreadsheetDocument).icon_name
           == "table_chart");
    assert(icons::source_attribution(
               icons::Symbol::PresentationDocument).icon_name
           == "slideshow");
    assert(icons::source_attribution(icons::Symbol::Search).family
           == "Google Material Symbols");
    assert(icons::source_attribution(icons::Symbol::Search).icon_name
           == "search");
    assert(icons::source_attribution(icons::Symbol::Search).license
           == "Apache-2.0");
    assert(icons::source_attribution(icons::Symbol::Back).license
           == "Apache-2.0");
    assert(icons::source_attribution(icons::Symbol::Plus).license
           == "Apache-2.0");
    assert(icons::source_attribution(icons::Symbol::Folder).license
           == "Apache-2.0");
    assert(icons::source_attribution(icons::Symbol::AudioDocument).license
           == "Apache-2.0");
    assert(icons::sidebar_symbol_at(8) == icons::Symbol::AirDrop);
    assert(icons::toolbar_symbol_at(10) == icons::Symbol::Search);
    assert(icons::file_type_symbol_at(0) == icons::Symbol::Folder);
    assert(icons::file_type_symbol_at(2) == icons::Symbol::PdfDocument);
    assert(icons::file_type_symbol_at(6) == icons::Symbol::Archive);
    assert(icons::file_type_symbol_at(7) == icons::Symbol::AudioDocument);
    assert(icons::file_type_symbol_at(8) == icons::Symbol::CodeDocument);
    assert(icons::file_type_symbol_at(9)
           == icons::Symbol::SpreadsheetDocument);
    assert(icons::file_type_symbol_at(10)
           == icons::Symbol::PresentationDocument);
    assert(icons::file_type_extension_rule_count == 36);
    assert(icons::is_file_type_symbol(icons::Symbol::Folder));
    assert(icons::is_file_type_symbol(icons::Symbol::PdfDocument));
    assert(!icons::is_file_type_symbol(icons::Symbol::Search));
    assert(icons::file_type_token(icons::Symbol::PdfDocument) == "pdf");
    assert(icons::file_type_token(icons::Symbol::Search) == "document");
    assert(icons::file_type_kind_label(icons::Symbol::PdfDocument)
           == "PDF Document");
    assert(icons::file_type_kind_label(icons::Symbol::Image) == "Image");
    assert(icons::file_type_extension_family(icons::Symbol::Image)
           == "raster_or_svg_image");
    assert(icons::file_type_symbol_from_token("presentation").has_value());
    assert(*icons::file_type_symbol_from_token("presentation")
           == icons::Symbol::PresentationDocument);
    assert(!icons::file_type_symbol_from_token("missing").has_value());
    auto const pdf_icon =
        icons::file_type_icon_descriptor(icons::Symbol::PdfDocument);
    assert(pdf_icon.symbol == icons::Symbol::PdfDocument);
    assert(pdf_icon.token == "pdf");
    assert(pdf_icon.kind_label == "PDF Document");
    assert(pdf_icon.package_asset_name == std::string{"file_type.pdf.icon"});
    assert(pdf_icon.package_asset_source
           == std::string{"assets/icons/file-types/pdf.svg"});
    auto const normalized_icon =
        icons::file_type_icon_descriptor(icons::Symbol::Search);
    assert(normalized_icon.symbol == icons::Symbol::Document);
    assert(icons::file_extension_is_svg_image("SVG"));
    assert(icons::file_extension_is_raster_image("heic"));
    assert(icons::file_extension_is_image("svgz"));
    assert(icons::file_extension_is_movie("MOV"));
    assert(icons::file_extension_is_audio("m4a"));
    assert(icons::file_extension_is_code("CPP"));
    assert(icons::file_extension_is_spreadsheet("numbers"));
    assert(icons::file_extension_is_presentation("key"));
    assert(icons::file_extension_is_archive("zip"));
    assert(icons::file_type_symbol_for_extension("pdf")
           == icons::Symbol::PdfDocument);
    assert(icons::file_type_symbol_for_extension("svg")
           == icons::Symbol::Image);
    assert(icons::file_type_symbol_for_extension("mov")
           == icons::Symbol::Movie);
    assert(icons::file_type_symbol_for_extension("m4a")
           == icons::Symbol::AudioDocument);
    assert(icons::file_type_symbol_for_extension("cpp")
           == icons::Symbol::CodeDocument);
    assert(icons::file_type_symbol_for_extension("csv")
           == icons::Symbol::SpreadsheetDocument);
    assert(icons::file_type_symbol_for_extension("pptx")
           == icons::Symbol::PresentationDocument);
    assert(icons::file_type_symbol_for_extension("unknown")
           == icons::Symbol::Document);
    assert(icons::file_type_symbol_for_entry("unknown", true)
           == icons::Symbol::Folder);
    assert(icons::known_file_type_kind_label_for_extension("svg")
           .has_value());
    assert(*icons::known_file_type_kind_label_for_extension("svg")
           == std::string_view{"SVG Image"});
    assert(*icons::known_file_type_kind_label_for_extension("png")
           == std::string_view{"Image"});
    assert(!icons::known_file_type_kind_label_for_extension("unknown")
            .has_value());
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
    auto const toolbar_chrome = icons::macos_control_chrome(
        icons::SymbolPresentationRole::Toolbar,
        icons::SymbolInteractionState{false, true});
    assert(toolbar_chrome.symbol_tone == icons::SymbolTone::Secondary);
    assert(toolbar_chrome.background_color.a == 0);
    assert(toolbar_chrome.hover_background_color.a == 120);
    assert(toolbar_chrome.corner_radius == 15.0f);
    assert(toolbar_chrome.borderless);
    auto const selected_toolbar_chrome = icons::macos_control_chrome(
        icons::SymbolPresentationRole::Toolbar,
        icons::SymbolInteractionState{true, true});
    assert(selected_toolbar_chrome.background_color.a == 150);
    assert(selected_toolbar_chrome.hover_background_color.a == 190);
    auto const selected_sidebar_chrome = icons::macos_control_chrome(
        icons::SymbolPresentationRole::Sidebar,
        icons::SymbolInteractionState{true, true});
    assert(selected_sidebar_chrome.symbol_tone == icons::SymbolTone::Accent);
    assert(selected_sidebar_chrome.symbol_color.b == 255);
    assert(selected_sidebar_chrome.background_color.r == 236);
    assert(selected_sidebar_chrome.background_color.a == 150);
    assert(selected_sidebar_chrome.hover_background_color.r == 232);
    assert(selected_sidebar_chrome.hover_background_color.a == 176);
    assert(selected_sidebar_chrome.corner_radius == 10.0f);
    auto const disabled_toolbar_chrome = icons::macos_control_chrome(
        icons::SymbolPresentationRole::Toolbar,
        icons::SymbolInteractionState{false, false});
    assert(disabled_toolbar_chrome.symbol_tone == icons::SymbolTone::Disabled);
    assert(disabled_toolbar_chrome.hover_background_color.a == 0);
    auto const toolbar_pressed_recipe = icons::macos_state_recipe(
        icons::SymbolPresentationRole::Toolbar,
        icons::SymbolInteractionState{false, true},
        icons::SymbolInteractionPhase::Pressed);
    assert(toolbar_pressed_recipe.phase
           == icons::SymbolInteractionPhase::Pressed);
    assert(toolbar_pressed_recipe.background_color.a == 150);
    assert(toolbar_pressed_recipe.symbol_opacity < 1.0f);
    assert(toolbar_pressed_recipe.scale < 1.0f);
    assert(toolbar_pressed_recipe.policy
           == icons::symbol_interaction_phase_policy());
    auto const selected_sidebar_pressed_recipe = icons::macos_state_recipe(
        icons::SymbolPresentationRole::Sidebar,
        icons::SymbolInteractionState{true, true},
        icons::SymbolInteractionPhase::Pressed);
    assert(selected_sidebar_pressed_recipe.symbol_tone
           == icons::SymbolTone::Accent);
    assert(selected_sidebar_pressed_recipe.background_color.a == 196);
    assert(selected_sidebar_pressed_recipe.scale < 1.0f);
    auto const disabled_recipe = icons::macos_state_recipe(
        icons::SymbolPresentationRole::Toolbar,
        icons::SymbolInteractionState{false, false},
        icons::SymbolInteractionPhase::Hovered);
    assert(disabled_recipe.symbol_tone == icons::SymbolTone::Disabled);
    assert(disabled_recipe.background_color.a == 0);
    assert(disabled_recipe.symbol_color.r == 142);
    assert(disabled_recipe.symbol_opacity >= 0.75f);
    assert(icons::file_type_color_policy() == "macos_finder_file_type_tints");
    auto const folder_color = icons::macos_file_type_color(icons::Symbol::Folder);
    auto const image_color = icons::macos_file_type_color(icons::Symbol::Image);
    auto const pdf_color =
        icons::macos_file_type_color(icons::Symbol::PdfDocument);
    auto const archive_color =
        icons::macos_file_type_color(icons::Symbol::Archive);
    auto const spreadsheet_color =
        icons::macos_file_type_color(icons::Symbol::SpreadsheetDocument);
    auto const presentation_color =
        icons::macos_file_type_color(icons::Symbol::PresentationDocument);
    auto const document_color =
        icons::macos_file_type_color(icons::Symbol::Document);
    assert(folder_color.b > folder_color.r);
    assert(image_color.g > document_color.g);
    assert(pdf_color.r > pdf_color.g);
    assert(archive_color.r > archive_color.b);
    assert(spreadsheet_color.g > spreadsheet_color.r);
    assert(presentation_color.r > presentation_color.b);
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
    assert(icons::activation_hit_target_size(
               icons::SymbolPresentationRole::Toolbar)
           == 44.0f);
    assert(icons::activation_hit_target_size(
               icons::SymbolPresentationRole::Sidebar)
           == 44.0f);
    assert(icons::activation_hit_target_size(
               icons::SymbolPresentationRole::FileType)
           == 64.0f);
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

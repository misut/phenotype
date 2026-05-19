#include <cassert>
#include <filesystem>
#include <string>

import phenotype.icon_catalog;
import phenotype_cli.icons_common;
import phenotype_cli.icons_render;

int main() {
    namespace icon_catalog = phenotype::icon_catalog;
    namespace cli_icons = phenotype_cli::icons;

    auto synthetic_source = icon_catalog::SymbolSourceAttribution{
        .family = "Synthetic",
        .icon_name = "probe",
        .license = "MIT",
        .license_url = "https://example.invalid/license",
        .source_url = "https://example.invalid/probe.svg",
        .source_revision = "unit-test",
        .copyright = "Copyright 2026",
        .embedded_source = true,
        .modified_for_phenotype = true,
        .apple_asset = false,
        .platform_extracted = true,
        .runtime_fetch_required = true,
    };
    auto source_json = cli_icons::icon_source_attribution_json(
        synthetic_source);
    assert(source_json.find("\"apple_asset\":false") != std::string::npos);
    assert(source_json.find("\"platform_extracted\":true")
        != std::string::npos);
    assert(source_json.find("\"runtime_fetch_required\":true")
        != std::string::npos);

    auto lookup = cli_icons::lookup_icon_symbol("search");
    assert(lookup.has_value());
    assert(lookup->symbol == icon_catalog::Symbol::Search);
    assert(lookup->match_kind == std::string_view{"name"});

    auto rendered = cli_icons::rendered_icon_svg_source(
        *lookup,
        icon_catalog::SymbolPresentationRole::Toolbar,
        icon_catalog::SymbolInteractionPhase::Pressed,
        false,
        true);
    assert(rendered.find("data-phenotype-symbol=\"search\"")
        != std::string::npos);
    assert(rendered.find("data-role=\"toolbar\"") != std::string::npos);
    assert(rendered.find("data-phase=\"pressed\"") != std::string::npos);

    auto json = cli_icons::icon_render_json(
        "search",
        *lookup,
        icon_catalog::SymbolPresentationRole::Toolbar,
        icon_catalog::SymbolInteractionPhase::Pressed,
        false,
        true,
        rendered,
        std::filesystem::path{});
    assert(json.find("\"likely_pass\":\"standalone_svg_wrapper\"")
        != std::string::npos);
    assert(json.find("\"output\":null") != std::string::npos);
}

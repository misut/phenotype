#include <array>
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>

import phenotype.resources;

namespace {

[[noreturn]] void fail_assert(char const* expression, char const* file, int line) {
    std::cerr << "FAIL: " << file << ":" << line << ": " << expression << "\n";
    std::exit(1);
}

} // namespace

#undef assert
#define assert(expression) \
    ((expression) ? static_cast<void>(0) : fail_assert(#expression, __FILE__, __LINE__))

int main() {
    phenotype::ResourceCatalog catalog;
    catalog.application.id = "com.misut.phenotype.test";
    catalog.application.display_name = "Resource Test";
    catalog.application.entry = "resource_test";
    catalog.default_locale = "ko";
    catalog.default_font_family = "Pretendard";
    catalog.assets.push_back({
        .name = "app.icon",
        .source = "assets/icon.svg",
        .content_type = "image/svg+xml",
        .preload = true,
    });
    catalog.locales.push_back({
        .tag = "en",
        .source = "locales/en.toml",
        .strings = {
            {.key = "app.title", .value = "Files"},
            {.key = "actions.delete", .value = "Move to Trash"},
        },
    });
    catalog.locales.push_back({
        .tag = "ko",
        .source = "locales/ko.toml",
        .fallback = {"en"},
        .strings = {
            {.key = "app.title", .value = "파일"},
        },
    });
    catalog.fonts.push_back({
        .family = "Pretendard",
        .source = "fonts/pretendard.alias.toml",
        .fallback = {"system-ui", "Noto Sans CJK"},
    });
    catalog.debug.artifact_manifest = "artifact_manifest.json";
    catalog.debug.verifier = "tools/verify_file_explorer_artifacts.sh";

    auto fallback = phenotype::localized_string(
        catalog,
        "actions.delete",
        "ko");
    assert(fallback);
    assert(fallback->tag == "en");
    assert(fallback->fallback);

    auto required = std::array<std::string_view, 2>{
        "app.title",
        "actions.delete",
    };
    auto diagnostics = phenotype::validate_resource_catalog(catalog, required);
    assert(diagnostics.empty());
    auto contract = phenotype::resource_catalog_contract(catalog, required);
    assert(contract.asset_count == 1);
    assert(contract.preload_asset_count == 1);
    assert(contract.runtime_visible_asset_count == 0);
    assert(contract.svg_asset_count == 1);
    assert(contract.preload_svg_asset_count == 1);
    assert(contract.runtime_visible_svg_asset_count == 0);
    assert(contract.locale_count == 2);
    assert(contract.locale_string_count == 3);
    assert(contract.font_count == 1);
    assert(contract.app_icon_declared);
    assert(contract.app_icon_svg);
    assert(contract.app_icon_preload);
    assert(contract.default_locale_declared);
    assert(contract.default_font_declared);
    assert(contract.default_font_has_cjk_fallback);
    assert(contract.debug_artifact_manifest_declared);
    assert(contract.debug_verifier_declared);
    assert(phenotype::resource_asset_declares_svg(catalog.assets[0]));
    assert(phenotype::svg_asset_contract_policy()
           == "package_svg_assets_must_declare_image_svg_xml_and_svg_source_suffix");
    assert(contract.locale_coverage.size() == 2);
    assert(contract.locale_coverage[1].tag == "ko");
    assert(contract.locale_coverage[1].fallback_chain.size() == 2);
    assert(contract.locale_coverage[1].required_key_count == 2);
    assert(contract.locale_coverage[1].resolved_key_count == 2);
    assert(contract.locale_coverage[1].missing_keys.empty());

    auto parsed = phenotype::parse_resource_manifest(R"(
[application]
id = "com.misut.phenotype.parser-test"
display_name = "Parser Test"
version = "1.2.3"
platforms = ["macos", "windows"]
entry = "parser_test"

[resources]
default_locale = "en"
default_font_family = "Pretendard"
artifact_manifest = "artifact_manifest.json"

[[assets]]
name = "app.icon"
source = "assets/icon.svg"
content_type = "image/svg+xml"
preload = true
runtime_visible = false

[[locales]]
tag = "en"
source = "locales/en.toml"
fallback = []

[[locales]]
tag = "ko"
source = "locales/ko.toml"
fallback = ["en"]

[[fonts]]
family = "Pretendard"
source = "fonts/pretendard.alias.toml"
register = false
fallback = ["system-ui", "Apple SD Gothic Neo"]

[debug]
artifact_manifest = "artifact_manifest.json"
probe_scene = "parser-probe"
verifier = "tools/verify_file_explorer_artifacts.sh"
)");
    assert(parsed.application_section);
    assert(parsed.resources_section);
    assert(parsed.debug_section);
    assert(parsed.artifact_manifest == "artifact_manifest.json");
    assert(parsed.source_references.size() == 4);
    assert(parsed.catalog.application.id == "com.misut.phenotype.parser-test");
    assert(parsed.catalog.application.platforms.size() == 2);
    assert(parsed.catalog.assets.size() == 1);
    assert(parsed.catalog.assets[0].preload);
    assert(!parsed.catalog.assets[0].runtime_visible);
    assert(parsed.catalog.locales.size() == 2);
    assert(parsed.catalog.locales[1].fallback.size() == 1);
    assert(parsed.catalog.fonts.size() == 1);
    assert(parsed.catalog.fonts[0].fallback.size() == 2);
    assert(parsed.catalog.debug.probe_scene == "parser-probe");

    parsed.catalog.locales[0].strings =
        phenotype::parse_resource_locale_strings(R"(
title = "Files"
[actions]
delete = "Move to Trash"
)");
    assert(parsed.catalog.locales[0].strings.size() == 2);
    assert(parsed.catalog.locales[0].strings[0].key == "title");
    assert(parsed.catalog.locales[0].strings[1].key == "actions.delete");

    auto has_kind = [](std::vector<phenotype::ResourceDiagnostic> const& values,
                       phenotype::ResourceDiagnosticKind kind) {
        return std::ranges::any_of(values, [&](auto const& diagnostic) {
            return diagnostic.kind == kind;
        });
    };

    auto invalid_policy_catalog = catalog;
    invalid_policy_catalog.assets[0].source = "assets/icon.txt";
    invalid_policy_catalog.assets[0].content_type = "text/plain";
    invalid_policy_catalog.fonts[0].fallback = {"system-ui"};
    diagnostics =
        phenotype::validate_resource_catalog(invalid_policy_catalog, required);
    assert(has_kind(
        diagnostics,
        phenotype::ResourceDiagnosticKind::InvalidAppIconSvg));
    assert(has_kind(
        diagnostics,
        phenotype::ResourceDiagnosticKind::MissingDefaultFontCjkFallback));

    catalog.assets.push_back({
        .name = "app.icon",
        .source = "assets/duplicate.svg",
        .content_type = "image/svg+xml",
    });
    diagnostics = phenotype::validate_resource_catalog(catalog, required);
    auto duplicate = false;
    for (auto const& diagnostic : diagnostics) {
        duplicate = duplicate
            || diagnostic.kind == phenotype::ResourceDiagnosticKind::DuplicateAssetName;
    }
    assert(duplicate);

    std::puts("PASS: phenotype resource catalog");
}

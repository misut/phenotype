export module phenotype_cli.package_json;

import phenotype.resources;
import phenotype_cli.common;
import phenotype_cli.package_inspect;
import phenotype_cli.package_svg;
export import phenotype_cli.package_types;
import std;

export namespace phenotype_cli::package {

using namespace phenotype_cli::common;

auto locale_key_array_json(
        std::span<phenotype::LocaleString const> strings) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < strings.size(); ++i) {
        if (i > 0)
            out += ",";
        out += json_string(strings[i].key);
    }
    out += "]";
    return out;
}

auto assets_catalog_json(
        std::span<phenotype::AssetDescriptor const> assets) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < assets.size(); ++i) {
        auto const& asset = assets[i];
        if (i > 0)
            out += ",";
        out += std::format(
            "{{\"name\":{},\"source\":{},\"content_type\":{},"
            "\"svg\":{},\"preload\":{},\"runtime_visible\":{}}}",
            json_string(asset.name),
            json_string(asset.source),
            json_string(asset.content_type),
            phenotype::resource_asset_declares_svg(asset) ? "true" : "false",
            asset.preload ? "true" : "false",
            asset.runtime_visible ? "true" : "false");
    }
    out += "]";
    return out;
}

auto locales_catalog_json(
        std::span<phenotype::LocaleDescriptor const> locales) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < locales.size(); ++i) {
        auto const& locale = locales[i];
        if (i > 0)
            out += ",";
        out += std::format(
            "{{\"tag\":{},\"source\":{},\"fallback\":{},"
            "\"string_count\":{},\"keys\":{}}}",
            json_string(locale.tag),
            json_string(locale.source),
            string_array_json(locale.fallback),
            locale.strings.size(),
            locale_key_array_json(locale.strings));
    }
    out += "]";
    return out;
}

auto fonts_catalog_json(
        std::span<phenotype::FontDescriptor const> fonts) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < fonts.size(); ++i) {
        auto const& font = fonts[i];
        if (i > 0)
            out += ",";
        out += std::format(
            "{{\"family\":{},\"source\":{},\"register\":{},"
            "\"fallback\":{}}}",
            json_string(font.family),
            json_string(font.source),
            font.register_font ? "true" : "false",
            string_array_json(font.fallback));
    }
    out += "]";
    return out;
}

auto locale_coverage_json(
        std::span<phenotype::LocaleCoverage const> locales) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < locales.size(); ++i) {
        auto const& locale = locales[i];
        if (i > 0)
            out += ",";
        out += std::format(
            "{{\"tag\":{},\"default_locale\":{},\"fallback_chain\":{},"
            "\"declared_string_count\":{},\"required_key_count\":{},"
            "\"resolved_key_count\":{},\"missing_keys\":{},\"ok\":{}}}",
            json_string(locale.tag),
            locale.default_locale ? "true" : "false",
            string_array_json(locale.fallback_chain),
            locale.declared_string_count,
            locale.required_key_count,
            locale.resolved_key_count,
            string_array_json(locale.missing_keys),
            locale.missing_keys.empty() ? "true" : "false");
    }
    out += "]";
    return out;
}

auto resource_contract_json(PackageSummary const& summary) -> std::string {
    auto const& contract = summary.resource_contract;
    return std::format(
        "{{\"asset_count\":{},\"preload_asset_count\":{},"
        "\"runtime_visible_asset_count\":{},"
        "\"svg_asset_count\":{},\"preload_svg_asset_count\":{},"
        "\"runtime_visible_svg_asset_count\":{},"
        "\"svg_asset_policy\":{},\"locale_count\":{},"
        "\"locale_string_count\":{},\"font_count\":{},"
        "\"registered_font_count\":{},"
        "\"app_icon_declared\":{},\"app_icon_svg\":{},"
        "\"app_icon_preload\":{},"
        "\"default_locale_declared\":{},\"default_font_declared\":{},"
        "\"default_font_has_cjk_fallback\":{},"
        "\"debug_artifact_manifest_declared\":{},"
        "\"debug_probe_scene_declared\":{},\"debug_verifier_declared\":{},"
        "\"required_locale_key_count\":{},\"missing_locale_key_count\":{},"
        "\"locale_coverage\":{},\"ok\":{}}}",
        contract.asset_count,
        contract.preload_asset_count,
        contract.runtime_visible_asset_count,
        contract.svg_asset_count,
        contract.preload_svg_asset_count,
        contract.runtime_visible_svg_asset_count,
        json_string(phenotype::svg_asset_contract_policy()),
        contract.locale_count,
        contract.locale_string_count,
        contract.font_count,
        contract.registered_font_count,
        contract.app_icon_declared ? "true" : "false",
        contract.app_icon_svg ? "true" : "false",
        contract.app_icon_preload ? "true" : "false",
        contract.default_locale_declared ? "true" : "false",
        contract.default_font_declared ? "true" : "false",
        contract.default_font_has_cjk_fallback ? "true" : "false",
        contract.debug_artifact_manifest_declared ? "true" : "false",
        contract.debug_probe_scene_declared ? "true" : "false",
        contract.debug_verifier_declared ? "true" : "false",
        summary.required_locale_key_count,
        locale_coverage_missing_key_count(summary),
        locale_coverage_json(contract.locale_coverage),
        (resource_contract_ok(summary) && locale_coverage_ok(summary))
            ? "true"
            : "false");
}

auto resource_catalog_json(PackageSummary const& summary) -> std::string {
    auto const& catalog = summary.catalog;
    return std::format(
        "{{\"application\":{{\"id\":{},\"display_name\":{},\"version\":{},"
        "\"entry\":{},\"platforms\":{}}},"
        "\"defaults\":{{\"locale\":{},\"font_family\":{}}},"
        "\"assets\":{},\"svg_asset_inspections\":{},"
        "\"locales\":{},\"fonts\":{},"
        "\"debug\":{{\"artifact_manifest\":{},\"probe_scene\":{},"
        "\"verifier\":{}}},\"diagnostics\":{}}}",
        json_string(catalog.application.id),
        json_string(catalog.application.display_name),
        json_string(catalog.application.version),
        json_string(catalog.application.entry),
        string_array_json(catalog.application.platforms),
        json_string(catalog.default_locale),
        json_string(catalog.default_font_family),
        assets_catalog_json(catalog.assets),
        svg_asset_inspections_json(summary.svg_asset_inspections),
        locales_catalog_json(catalog.locales),
        fonts_catalog_json(catalog.fonts),
        json_string(catalog.debug.artifact_manifest),
        json_string(catalog.debug.probe_scene),
        json_string(catalog.debug.verifier),
        resource_diagnostics_json(summary.catalog_diagnostics));
}

auto resource_catalog_summary_json(PackageSummary const& summary)
    -> std::string {
    auto const& catalog = summary.catalog;
    return std::format(
        "{{\"default_locale\":{},\"default_font_family\":{},"
        "\"assets\":{},\"svg_assets\":{},"
        "\"svg_assets_inspected\":{},\"svg_asset_failures\":{},"
        "\"locales\":{},\"locale_strings\":{},"
        "\"fonts\":{},\"diagnostics\":{},"
        "\"preload_assets\":{},\"runtime_visible_assets\":{},"
        "\"app_icon_declared\":{},\"app_icon_svg\":{},"
        "\"default_font_has_cjk_fallback\":{},"
        "\"missing_locale_keys\":{},\"contract_ok\":{}}}",
        json_string(catalog.default_locale),
        json_string(catalog.default_font_family),
        catalog.assets.size(),
        summary.resource_contract.svg_asset_count,
        summary.svg_asset_inspections.size(),
        svg_asset_inspection_failure_count(summary),
        catalog.locales.size(),
        summary.locale_string_count,
        catalog.fonts.size(),
        summary.catalog_diagnostics.size(),
        summary.resource_contract.preload_asset_count,
        summary.resource_contract.runtime_visible_asset_count,
        summary.resource_contract.app_icon_declared ? "true" : "false",
        summary.resource_contract.app_icon_svg ? "true" : "false",
        summary.resource_contract.default_font_has_cjk_fallback
            ? "true"
            : "false",
        locale_coverage_missing_key_count(summary),
        (resource_contract_ok(summary) && locale_coverage_ok(summary))
            ? "true"
            : "false");
}

auto package_json(PackageSummary const& summary,
                  std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":\"package inspect\",\"ok\":{},"
        "\"root\":{},"
        "\"manifest\":{{\"present\":{},\"bytes\":{},"
        "\"application\":{{\"id\":{},\"display_name\":{},\"version\":{},"
        "\"entry\":{},\"platforms\":{}}},"
        "\"sections\":{{\"application\":{},\"resources\":{},\"debug\":{}}},"
        "\"declared_resources\":{{\"assets\":{},\"svg_assets\":{},"
        "\"locales\":{},\"fonts\":{}}},"
        "\"source_reference_count\":{},\"missing_sources\":{},"
        "\"locale_string_count\":{},"
        "\"artifact_manifest\":{{\"path\":{},\"present\":{}}},"
        "\"debug\":{{\"probe_scene\":{},\"verifier\":{}}},"
        "\"app_icon\":{{\"declared\":{},\"svg\":{},\"preload\":{}}},"
        "\"default_font_family\":{},\"default_font_pretendard\":{},"
        "\"default_font_has_cjk_fallback\":{}}},"
        "\"assets\":{{\"present\":{},\"file_count\":{}}},"
        "\"svg_asset_inspections\":{},"
        "\"locales\":{{\"present\":{},\"file_count\":{}}},"
        "\"fonts\":{{\"present\":{},\"file_count\":{}}},"
        "\"resource_catalog\":{},\"resource_contract\":{},\"checks\":{}}}",
        all_ok(checks) ? "true" : "false",
        json_string(path_string(summary.root)),
        summary.manifest ? "true" : "false",
        summary.manifest_bytes,
        json_string(summary.application_id),
        json_string(summary.display_name),
        json_string(summary.version),
        json_string(summary.entry),
        string_array_json(summary.platforms),
        summary.application_section ? "true" : "false",
        summary.resources_section ? "true" : "false",
        summary.debug_section ? "true" : "false",
        summary.manifest_asset_count,
        summary.resource_contract.svg_asset_count,
        summary.manifest_locale_count,
        summary.manifest_font_count,
        summary.source_reference_count,
        string_array_json(summary.missing_sources),
        summary.locale_string_count,
        json_string(summary.artifact_manifest),
        summary.artifact_manifest_exists ? "true" : "false",
        json_string(summary.debug_probe_scene),
        json_string(summary.debug_verifier),
        summary.app_icon_declared ? "true" : "false",
        summary.app_icon_svg ? "true" : "false",
        summary.app_icon_preload ? "true" : "false",
        json_string(summary.default_font_family),
        summary.default_font_pretendard ? "true" : "false",
        summary.default_font_has_cjk_fallback ? "true" : "false",
        summary.assets_directory ? "true" : "false",
        summary.asset_file_count,
        svg_asset_inspections_json(summary.svg_asset_inspections),
        summary.locales_directory ? "true" : "false",
        summary.locale_file_count,
        summary.fonts_directory ? "true" : "false",
        summary.font_file_count,
        resource_catalog_json(summary),
        resource_contract_json(summary),
        checks_json(checks));
}

auto package_entry_json(PackageSummary const& summary,
                        std::span<Check const> checks) -> std::string {
    return std::format(
        "{{\"root\":{},\"ok\":{},\"application_id\":{},"
        "\"display_name\":{},\"entry\":{},\"platforms\":{},"
        "\"default_font_family\":{},\"artifact_manifest\":{},"
        "\"debug_verifier\":{},\"app_icon\":{{\"declared\":{},"
        "\"svg\":{},\"preload\":{}}},\"resource_catalog\":{},"
        "\"resource_contract\":{},\"checks\":{}}}",
        json_string(path_string(summary.root)),
        all_ok(checks) ? "true" : "false",
        json_string(summary.application_id),
        json_string(summary.display_name),
        json_string(summary.entry),
        string_array_json(summary.platforms),
        json_string(summary.default_font_family),
        json_string(summary.artifact_manifest),
        json_string(summary.debug_verifier),
        summary.app_icon_declared ? "true" : "false",
        summary.app_icon_svg ? "true" : "false",
        summary.app_icon_preload ? "true" : "false",
        resource_catalog_summary_json(summary),
        resource_contract_json(summary),
        checks_json(checks));
}

} // namespace phenotype_cli::package

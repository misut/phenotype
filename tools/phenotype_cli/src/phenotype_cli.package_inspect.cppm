export module phenotype_cli.package_inspect;

import phenotype.resources;
import phenotype_cli.common;
import phenotype_cli.package_svg;
export import phenotype_cli.package_types;
import std;

export namespace phenotype_cli::package {

namespace fs = std::filesystem;
using namespace phenotype_cli::common;

auto parse_locale_strings(fs::path const& path)
    -> std::vector<phenotype::LocaleString> {
    return phenotype::parse_resource_locale_strings(read_text_file(path));
}

auto package_summary(fs::path root) -> PackageSummary {
    auto summary = PackageSummary{.root = std::move(root)};
    summary.exists = path_exists(summary.root);
    summary.is_directory = path_is_directory(summary.root);
    summary.catalog.default_locale = "en";
    summary.catalog.default_font_family = "Pretendard";
    auto manifest_path = summary.root / "phenotype.package.toml";
    summary.manifest = path_exists(manifest_path);
    summary.manifest_bytes = file_size_or_zero(manifest_path);
    summary.assets_directory = path_is_directory(summary.root / "assets");
    summary.locales_directory = path_is_directory(summary.root / "locales");
    summary.fonts_directory = path_is_directory(summary.root / "fonts");
    summary.asset_file_count = count_regular_files(summary.root / "assets");
    summary.locale_file_count = count_regular_files(summary.root / "locales");
    summary.font_file_count = count_regular_files(summary.root / "fonts");

    if (summary.manifest) {
        auto text = read_text_file(manifest_path);
        auto parsed = phenotype::parse_resource_manifest(text);
        summary.catalog = std::move(parsed.catalog);
        summary.application_section = parsed.application_section;
        summary.resources_section = parsed.resources_section;
        summary.debug_section = parsed.debug_section;
        summary.artifact_manifest = std::move(parsed.artifact_manifest);
        summary.artifact_manifest_exists = !summary.artifact_manifest.empty()
            && path_exists(summary.root / summary.artifact_manifest);
        summary.source_reference_count = parsed.source_references.size();
        for (auto const& source : parsed.source_references) {
            if (!path_exists(summary.root / source))
                summary.missing_sources.push_back(source);
        }
    }
    summary.application_id = summary.catalog.application.id;
    summary.display_name = summary.catalog.application.display_name;
    summary.version = summary.catalog.application.version;
    summary.entry = summary.catalog.application.entry;
    summary.platforms = summary.catalog.application.platforms;
    summary.default_font_family = summary.catalog.default_font_family;
    summary.default_font_pretendard =
        summary.catalog.default_font_family == "Pretendard";
    summary.debug_probe_scene = summary.catalog.debug.probe_scene;
    summary.debug_verifier = summary.catalog.debug.verifier;
    summary.manifest_asset_count = summary.catalog.assets.size();
    summary.manifest_locale_count = summary.catalog.locales.size();
    summary.manifest_font_count = summary.catalog.fonts.size();
    summary.svg_asset_inspections =
        inspect_package_svg_assets(summary.root, summary.catalog);

    for (auto& locale : summary.catalog.locales) {
        if (locale.source.empty())
            continue;
        auto source_path = summary.root / locale.source;
        if (!path_exists(source_path))
            continue;
        locale.strings = parse_locale_strings(source_path);
        summary.locale_string_count += locale.strings.size();
    }
    if (summary.catalog.debug.artifact_manifest.empty()
        && !summary.artifact_manifest.empty()) {
        summary.catalog.debug.artifact_manifest = summary.artifact_manifest;
    }

    auto required_keys = std::vector<std::string>{};
    if (auto locale = phenotype::find_locale(
            summary.catalog,
            summary.catalog.default_locale)) {
        for (auto const& text : locale->get().strings)
            required_keys.push_back(text.key);
    }
    auto required_views = std::vector<std::string_view>{};
    required_views.reserve(required_keys.size());
    for (auto const& key : required_keys)
        required_views.push_back(key);
    summary.required_locale_key_count = required_views.size();
    summary.catalog_diagnostics =
        phenotype::validate_resource_catalog(summary.catalog, required_views);
    summary.resource_contract =
        phenotype::resource_catalog_contract(summary.catalog, required_views);
    summary.app_icon_declared = summary.resource_contract.app_icon_declared;
    summary.app_icon_svg = summary.resource_contract.app_icon_svg;
    summary.app_icon_preload = summary.resource_contract.app_icon_preload;
    summary.default_font_has_cjk_fallback =
        summary.resource_contract.default_font_has_cjk_fallback;
    return summary;
}

bool debug_verifier_uses_cli(std::string_view verifier) {
    return verifier.starts_with("phenotype ");
}

auto locale_coverage_missing_key_count(PackageSummary const& summary)
    -> std::size_t {
    auto count = std::size_t{0};
    for (auto const& coverage : summary.resource_contract.locale_coverage)
        count += coverage.missing_keys.size();
    return count;
}

bool locale_coverage_ok(PackageSummary const& summary) {
    return summary.required_locale_key_count > 0
        && !summary.resource_contract.locale_coverage.empty()
        && locale_coverage_missing_key_count(summary) == 0;
}

bool resource_contract_ok(PackageSummary const& summary) {
    auto const& contract = summary.resource_contract;
    return contract.default_locale_declared
        && contract.default_font_declared
        && contract.default_font_has_cjk_fallback
        && contract.app_icon_declared
        && contract.app_icon_svg
        && contract.app_icon_preload
        && contract.debug_artifact_manifest_declared
        && contract.debug_probe_scene_declared
        && contract.debug_verifier_declared
        && contract.asset_count == summary.manifest_asset_count
        && contract.locale_count == summary.manifest_locale_count
        && contract.font_count == summary.manifest_font_count;
}

auto package_checks(PackageSummary const& summary) -> std::vector<Check> {
    return {
        {.name = "package_root",
         .ok = summary.exists && summary.is_directory,
         .detail = path_string(summary.root),
         .hint = "Pass a directory that owns phenotype package resources."},
        {.name = "manifest",
         .ok = summary.manifest && summary.manifest_bytes > 0,
         .detail = std::format("phenotype.package.toml ({} bytes)",
                               summary.manifest_bytes),
         .hint = "Add phenotype.package.toml before using CLI packaging."},
        {.name = "manifest_sections",
         .ok = summary.application_section && summary.resources_section
             && summary.debug_section,
         .detail = std::format("application={} resources={} debug={}",
                               summary.application_section ? "true" : "false",
                               summary.resources_section ? "true" : "false",
                               summary.debug_section ? "true" : "false"),
         .hint = "Expected [application], [resources], and [debug] sections."},
        {.name = "application_metadata",
         .ok = !summary.application_id.empty() && !summary.display_name.empty()
             && !summary.version.empty() && !summary.entry.empty()
             && !summary.platforms.empty(),
         .detail = std::format("id={} entry={} platforms={}",
                               summary.application_id.empty()
                                   ? "<missing>"
                                   : summary.application_id,
                               summary.entry.empty() ? "<missing>" : summary.entry,
                               summary.platforms.size()),
         .hint = "Expected application id, display_name, version, entry, and platforms."},
        {.name = "manifest_resources",
         .ok = summary.manifest_asset_count > 0
             && summary.manifest_locale_count > 0
             && summary.manifest_font_count > 0,
         .detail = std::format("assets={} locales={} fonts={}",
                               summary.manifest_asset_count,
                               summary.manifest_locale_count,
                               summary.manifest_font_count),
         .hint = "Expected at least one asset, locale, and font declaration."},
        {.name = "resource_catalog",
         .ok = summary.catalog_diagnostics.empty(),
         .detail = std::format("{} diagnostics, {} locale strings",
                               summary.catalog_diagnostics.size(),
                               summary.locale_string_count),
         .hint = "Fix the manifest fields reported by ResourceCatalog diagnostics."},
        {.name = "resource_contract",
         .ok = resource_contract_ok(summary),
         .detail = std::format(
             "default_locale={} default_font={} app_icon={} debug_manifest={} verifier={}",
             summary.resource_contract.default_locale_declared ? "true" : "false",
             summary.resource_contract.default_font_declared ? "true" : "false",
             summary.resource_contract.app_icon_declared ? "true" : "false",
             summary.resource_contract.debug_artifact_manifest_declared ? "true" : "false",
             summary.resource_contract.debug_verifier_declared ? "true" : "false"),
         .hint = "The pure ResourceCatalog contract should resolve defaults and debug metadata before launch."},
        {.name = "app_icon",
         .ok = summary.app_icon_declared && summary.app_icon_svg
             && summary.app_icon_preload,
         .detail = std::format("declared={} svg={} preload={}",
                               summary.app_icon_declared ? "true" : "false",
                               summary.app_icon_svg ? "true" : "false",
                               summary.app_icon_preload ? "true" : "false"),
         .hint = "Declare app.icon as a package-owned SVG asset with content_type image/svg+xml and preload=true."},
        {.name = "locale_coverage",
         .ok = locale_coverage_ok(summary),
         .detail = std::format("{} required keys, {} locales, {} missing",
                               summary.required_locale_key_count,
                               summary.resource_contract.locale_coverage.size(),
                               locale_coverage_missing_key_count(summary)),
         .hint = "Every declared locale fallback chain must resolve the default locale key set."},
        {.name = "resource_intent",
         .ok = summary.resource_contract.preload_asset_count > 0
             && summary.resource_contract.svg_asset_count > 0
             && summary.resource_contract.font_count > 0,
         .detail = std::format("preload_assets={} svg_assets={} runtime_visible_assets={} fonts={}",
                               summary.resource_contract.preload_asset_count,
                               summary.resource_contract.svg_asset_count,
                               summary.resource_contract.runtime_visible_asset_count,
                               summary.resource_contract.font_count),
         .hint = "Packages should declare preload intent, SVG image assets, and at least one UI font descriptor."},
        {.name = "svg_asset_inspection",
         .ok = svg_asset_inspections_ok(summary),
         .detail = std::format("{} inspected, {} failures",
                               summary.svg_asset_inspections.size(),
                               svg_asset_inspection_failure_count(summary)),
         .hint = "SVG package assets should be present, paintable, and free of unsupported-element diagnostics under the phenotype SVG subset."},
        {.name = "app_icon_native_chrome_palette",
         .ok = app_icon_native_window_palette_failure_count(summary) == 0,
         .detail = std::format("app_icon_marker_palette_failures={}",
                               app_icon_native_window_palette_failure_count(
                                   summary)),
         .hint = "Do not embed traffic-light/caption-button marker colors in packaged app icons; native windows own those controls."},
        {.name = "svg_assets_native_chrome_palette",
         .ok = native_window_palette_failure_count(summary) == 0,
         .detail = native_window_palette_failure_detail(summary),
         .hint = "Do not embed traffic-light/caption-button marker colors in packaged SVG assets; native windows own those controls."},
        {.name = "svg_file_type_icon_provenance",
         .ok = file_type_icon_provenance_ok(summary),
         .detail = file_type_icon_provenance_detail(summary),
         .hint = "File-type SVG package assets must be canonically source-equivalent to the audited icon catalog SVG, map to pinned source attribution, and avoid Apple artwork, platform extraction, and runtime fetch."},
        {.name = "manifest_sources",
         .ok = summary.source_reference_count > 0
             && summary.missing_sources.empty(),
         .detail = std::format("{} sources, {} missing",
                               summary.source_reference_count,
                               summary.missing_sources.size()),
         .hint = "Every manifest source path must exist relative to the package root."},
        {.name = "artifact_manifest",
         .ok = !summary.artifact_manifest.empty()
             && summary.artifact_manifest_exists,
         .detail = summary.artifact_manifest.empty()
             ? "artifact_manifest=<missing>"
             : std::format("{} present={}",
                           summary.artifact_manifest,
                           summary.artifact_manifest_exists ? "true" : "false"),
         .hint = "Package debug resources should point at an existing artifact manifest."},
        {.name = "debug_metadata",
         .ok = !summary.debug_probe_scene.empty()
             && !summary.debug_verifier.empty(),
         .detail = std::format("probe_scene={} verifier={}",
                               summary.debug_probe_scene.empty()
                                   ? "<missing>"
                                   : summary.debug_probe_scene,
                               summary.debug_verifier.empty()
                                   ? "<missing>"
                                   : summary.debug_verifier),
         .hint = "Expected [debug] probe_scene and verifier metadata."},
        {.name = "debug_verifier_cli",
         .ok = debug_verifier_uses_cli(summary.debug_verifier),
         .detail = summary.debug_verifier.empty()
             ? "verifier=<missing>"
             : std::format("verifier={}", summary.debug_verifier),
         .hint = "Use a phenotype CLI command; shell scripts are compatibility wrappers."},
        {.name = "default_font",
         .ok = summary.default_font_pretendard
             && summary.default_font_has_cjk_fallback,
         .detail = summary.default_font_family.empty()
             ? "default_font_family=<missing>"
             : std::format("default_font_family={} cjk_fallback={}",
                           summary.default_font_family,
                           summary.default_font_has_cjk_fallback
                               ? "true"
                               : "false"),
         .hint = "Package resources should default to Pretendard and declare a CJK-capable fallback for UI text."},
        {.name = "assets",
         .ok = summary.assets_directory,
         .detail = std::format("{} files", summary.asset_file_count),
         .hint = "Add assets/ for renderer-visible packaged resources."},
        {.name = "locales",
         .ok = summary.locales_directory,
         .detail = std::format("{} files", summary.locale_file_count),
         .hint = "Add locales/ for future i18n resources."},
        {.name = "fonts",
         .ok = summary.fonts_directory,
         .detail = std::format("{} files", summary.font_file_count),
         .hint = "Add fonts/ with Pretendard or aliases for portable UI text."},
    };
}

} // namespace phenotype_cli::package

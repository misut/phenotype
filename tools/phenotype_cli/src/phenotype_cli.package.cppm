export module phenotype_cli.package;

export import phenotype_cli.package_types;

import cppx.checksum;
import cppx.checksum.system;
import cppx.cli;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import json;
import phenotype.resources;
import phenotype_cli.common;
import phenotype_cli.package_svg;
import std;

namespace phenotype_cli::package {

namespace fs = std::filesystem;
using namespace phenotype_cli::common;

auto sha256_file_or_error(fs::path const& path)
    -> std::expected<std::string, std::string> {
    auto digest = cppx::checksum::system::sha256_file(path);
    if (digest)
        return *digest;
    return std::unexpected{std::format(
        "{}: {}",
        cppx::checksum::to_string(digest.error().code),
        digest.error().message)};
}

auto parse_locale_strings(fs::path const& path)
    -> std::vector<phenotype::LocaleString> {
    return phenotype::parse_resource_locale_strings(read_text_file(path));
}

auto first_positional_or_error(cppx::cli::Invocation const& invocation,
                               std::string_view command_name)
    -> std::expected<fs::path, std::string> {
    if (invocation.positionals.empty()) {
        return std::unexpected{
            std::format("{} requires one positional path", command_name)};
    }
    if (invocation.positionals.size() > 1) {
        return std::unexpected{
            std::format("{} accepts exactly one positional path", command_name)};
    }
    return fs::path{invocation.positionals.front()};
}

auto optional_positional_or_error(cppx::cli::Invocation const& invocation,
                                  std::string_view command_name,
                                  fs::path fallback)
    -> std::expected<fs::path, std::string> {
    if (invocation.positionals.empty())
        return fallback;
    if (invocation.positionals.size() > 1) {
        return std::unexpected{
            std::format("{} accepts at most one positional path", command_name)};
    }
    return fs::path{invocation.positionals.front()};
}

export auto package_summary(fs::path root) -> PackageSummary {
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

export auto package_checks(PackageSummary const& summary) -> std::vector<Check> {
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

auto bundle_relative_destination(BundleFileRecord const& file,
                                 fs::path const& output_root) -> std::string {
    auto ec = std::error_code{};
    auto relative_destination = fs::relative(file.destination, output_root, ec);
    return ec ? path_string(file.destination) : path_string(relative_destination);
}

auto bundle_file_json(BundleFileRecord const& file,
                      fs::path const& output_root) -> std::string {
    auto relative_destination = bundle_relative_destination(file, output_root);
    return std::format(
        "{{\"kind\":{},\"name\":{},\"content_type\":{},"
        "\"source\":{},\"destination\":{},\"bytes\":{},"
        "\"present\":{},\"copied\":{},\"preload\":{},"
        "\"runtime_visible\":{},\"integrity\":{{\"algorithm\":\"sha256\","
        "\"digest\":{},\"ok\":{},\"error\":{}}},\"error\":{}}}",
        json_string(file.kind),
        json_string(file.name),
        json_string(file.content_type),
        json_string(path_string(file.source)),
        json_string(relative_destination),
        file.bytes,
        file.present ? "true" : "false",
        file.copied ? "true" : "false",
        file.preload ? "true" : "false",
        file.runtime_visible ? "true" : "false",
        json_string(file.sha256),
        (!file.sha256.empty() && file.integrity_error.empty())
            ? "true" : "false",
        json_string(file.integrity_error),
        json_string(file.error));
}

auto bundle_file_integrity_ok(BundleFileRecord const& file,
                              bool require_copied) -> bool {
    return file.present
        && (!require_copied || file.copied)
        && file.error.empty()
        && file.integrity_error.empty()
        && !file.sha256.empty();
}

auto bundle_files_integrity_ok(std::span<BundleFileRecord const> files,
                               bool require_copied) -> bool {
    return std::ranges::all_of(files, [&](auto const& file) {
        return bundle_file_integrity_ok(file, require_copied);
    });
}

auto bundle_total_bytes(std::span<BundleFileRecord const> files)
    -> std::uintmax_t {
    auto total = std::uintmax_t{0};
    for (auto const& file : files) {
        if (file.present)
            total += file.bytes;
    }
    return total;
}

auto bundle_verified_file_count(std::span<BundleFileRecord const> files)
    -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(
        files,
        [](auto const& file) {
            return file.present
                && file.error.empty()
                && file.integrity_error.empty()
                && !file.sha256.empty();
        }));
}

auto bundle_files_json(std::span<BundleFileRecord const> files,
                       fs::path const& output_root) -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < files.size(); ++i) {
        if (i > 0)
            out += ",";
        out += bundle_file_json(files[i], output_root);
    }
    out += "]";
    return out;
}

auto stored_bundle_manifest_json(StoredBundleManifest const& manifest)
    -> std::string {
    return std::format(
        "{{\"present\":{},\"parse_ok\":{},\"parse_error\":{},"
        "\"schema_version\":{},\"command\":{},\"file_count\":{},"
        "\"total_bytes\":{},\"integrity_ok\":{},\"record_count\":{},"
        "\"matched_digest_count\":{},\"missing_record_count\":{},"
        "\"digest_mismatch_count\":{},\"ok\":{}}}",
        manifest.present ? "true" : "false",
        manifest.parse_ok ? "true" : "false",
        json_string(manifest.parse_error),
        manifest.schema_version,
        json_string(manifest.command),
        manifest.file_count,
        manifest.total_bytes,
        manifest.integrity_ok ? "true" : "false",
        manifest.records.size(),
        manifest.matched_digest_count,
        manifest.missing_record_count,
        manifest.digest_mismatch_count,
        (manifest.present && manifest.parse_ok
         && manifest.schema_version == 1
         && manifest.command == "package bundle"
         && manifest.integrity_ok
         && manifest.file_count == static_cast<std::int64_t>(manifest.records.size())
         && manifest.matched_digest_count == manifest.records.size()
         && manifest.missing_record_count == 0
         && manifest.digest_mismatch_count == 0)
            ? "true"
            : "false");
}

auto read_stored_bundle_manifest(fs::path const& manifest_path)
    -> StoredBundleManifest {
    auto manifest = StoredBundleManifest{};
    manifest.present = path_exists(manifest_path);
    if (!manifest.present) {
        manifest.parse_error = "phenotype.bundle.json is missing";
        return manifest;
    }

    try {
        auto parsed = json::parse(read_text_file(manifest_path));
        manifest.parse_ok = true;
        if (auto schema = json_integer_at(parsed, {"schema_version"}))
            manifest.schema_version = *schema;
        if (auto command = json_string_at(parsed, {"command"}))
            manifest.command = *command;
        if (auto count = json_integer_at(parsed, {"integrity", "file_count"}))
            manifest.file_count = *count;
        if (auto bytes = json_integer_at(parsed, {"integrity", "total_bytes"}))
            manifest.total_bytes = *bytes;
        if (auto ok = json_bool_at(parsed, {"integrity", "ok"}))
            manifest.integrity_ok = *ok;
        if (auto const* files = json_array_at(parsed, {"files"})) {
            for (auto const& file : *files) {
                auto destination = json_string_at(file, {"destination"});
                auto digest = json_string_at(file, {"integrity", "digest"});
                if (destination && digest) {
                    manifest.records.push_back({
                        .destination = *destination,
                        .digest = *digest,
                    });
                }
            }
        }
    } catch (std::exception const& error) {
        manifest.parse_error = error.what();
    }
    return manifest;
}

auto find_stored_bundle_record(StoredBundleManifest const& manifest,
                               std::string_view destination)
    -> BundleManifestRecord const* {
    auto found = std::ranges::find_if(
        manifest.records,
        [&](BundleManifestRecord const& record) {
            return record.destination == destination;
        });
    return found == manifest.records.end() ? nullptr : &*found;
}

void compare_stored_bundle_manifest(BundleSummary& summary) {
    summary.stored_manifest = read_stored_bundle_manifest(summary.manifest_path);
    auto& manifest = summary.stored_manifest;
    if (!manifest.parse_ok)
        return;

    for (auto const& file : summary.files) {
        auto destination = bundle_relative_destination(file, summary.output_root);
        auto const* record = find_stored_bundle_record(manifest, destination);
        if (!record) {
            ++manifest.missing_record_count;
            continue;
        }
        if (record->digest == file.sha256) {
            ++manifest.matched_digest_count;
        } else {
            ++manifest.digest_mismatch_count;
        }
    }
}

bool stored_bundle_manifest_ok(BundleSummary const& summary) {
    auto const& manifest = summary.stored_manifest;
    return manifest.present
        && manifest.parse_ok
        && manifest.schema_version == 1
        && manifest.command == "package bundle"
        && manifest.file_count == static_cast<std::int64_t>(summary.files.size())
        && manifest.total_bytes == static_cast<std::int64_t>(summary.total_bytes)
        && manifest.integrity_ok
        && manifest.records.size() == summary.files.size()
        && manifest.matched_digest_count == summary.files.size()
        && manifest.missing_record_count == 0
        && manifest.digest_mismatch_count == 0;
}

auto stored_bundle_manifest_check(BundleSummary const& summary) -> Check {
    auto const& manifest = summary.stored_manifest;
    return {
        .name = "bundle_manifest_contract",
        .ok = stored_bundle_manifest_ok(summary),
        .detail = std::format(
            "schema={} command={} files={}/{} bytes={}/{} digests={}/{} missing={} mismatched={}",
            manifest.schema_version,
            manifest.command.empty() ? "<missing>" : manifest.command,
            manifest.file_count,
            summary.files.size(),
            manifest.total_bytes,
            summary.total_bytes,
            manifest.matched_digest_count,
            summary.files.size(),
            manifest.missing_record_count,
            manifest.digest_mismatch_count),
        .hint = "Rebuild the staged bundle; phenotype.bundle.json must match every staged resource digest.",
    };
}

auto bundle_json(BundleSummary const& summary) -> std::string {
    return std::format(
        "{{\"schema_version\":1,\"command\":{},"
        "\"ok\":{},\"format\":{},\"package_root\":{},\"output_root\":{},"
        "\"resource_root\":{},"
        "\"bundle_manifest\":{},\"application\":{{\"id\":{},"
        "\"display_name\":{},\"version\":{},\"entry\":{},"
        "\"platforms\":{}}},\"defaults\":{{\"locale\":{},"
        "\"font_family\":{}}},\"debug\":{{\"artifact_manifest\":{},"
        "\"probe_scene\":{},\"verifier\":{}}},"
        "\"resource_contract\":{},"
        "\"integrity\":{{\"algorithm\":\"sha256\",\"ok\":{},"
        "\"file_count\":{},\"verified_file_count\":{},"
        "\"total_bytes\":{},\"bundle_manifest\":{{\"present\":{},"
        "\"bytes\":{}}}}},\"file_count\":{},"
        "\"files\":{},\"stored_manifest\":{},\"checks\":{},\"error\":{}}}",
        json_string(summary.command),
        summary.ok ? "true" : "false",
        json_string(summary.format),
        json_string(path_string(summary.package_root)),
        json_string(path_string(summary.output_root)),
        json_string(path_string(summary.resource_root)),
        json_string(path_string(summary.manifest_path)),
        json_string(summary.package.application_id),
        json_string(summary.package.display_name),
        json_string(summary.package.version),
        json_string(summary.package.entry),
        string_array_json(summary.package.platforms),
        json_string(summary.package.catalog.default_locale),
        json_string(summary.package.catalog.default_font_family),
        json_string(summary.package.catalog.debug.artifact_manifest),
        json_string(summary.package.catalog.debug.probe_scene),
        json_string(summary.package.catalog.debug.verifier),
        resource_contract_json(summary.package),
        bundle_files_integrity_ok(summary.files, summary.command == "package bundle")
            ? "true" : "false",
        summary.files.size(),
        summary.verified_file_count,
        summary.total_bytes,
        summary.bundle_manifest_present ? "true" : "false",
        summary.bundle_manifest_bytes,
        summary.files.size(),
        bundle_files_json(summary.files, summary.output_root),
        stored_bundle_manifest_json(summary.stored_manifest),
        checks_json(summary.checks),
        json_string(summary.error));
}

auto package_manifest_roots(fs::path root) -> std::vector<fs::path> {
    auto roots = std::vector<fs::path>{};
    if (!path_is_directory(root))
        return roots;

    auto ec = std::error_code{};
    auto options = fs::directory_options::skip_permission_denied;
    for (auto it = fs::recursive_directory_iterator(root, options, ec);
         !ec && it != fs::recursive_directory_iterator{};
         it.increment(ec)) {
        auto path = it->path();
        auto name = path.filename().string();
        if (it->is_directory(ec)) {
            if (name == ".git" || name == ".exon" || name == "build") {
                it.disable_recursion_pending();
            }
            continue;
        }
        ec.clear();
        if (it->is_regular_file(ec) && name == "phenotype.package.toml") {
            roots.push_back(path.parent_path());
        }
    }
    std::ranges::sort(roots);
    return roots;
}

auto xml_escape(std::string_view text) -> std::string {
    auto out = std::string{};
    out.reserve(text.size());
    for (char ch : text) {
        switch (ch) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            case '"': out += "&quot;"; break;
            case '\'': out += "&apos;"; break;
            default: out.push_back(ch); break;
        }
    }
    return out;
}

bool portable_bundle_file_name(std::string_view name) {
    if (name.empty() || name == "." || name == "..")
        return false;
    return std::ranges::all_of(name, [](char ch) {
        auto uch = static_cast<unsigned char>(ch);
        return std::isalnum(uch) || ch == '_' || ch == '-' || ch == '.';
    });
}

auto copy_bundle_file(fs::path const& package_root,
                      fs::path const& destination_root,
                      std::string kind,
                      std::string name,
                      fs::path relative_source,
                      std::string content_type = {},
                      bool preload = false,
                      bool runtime_visible = false) -> BundleFileRecord {
    BundleFileRecord record{
        .kind = std::move(kind),
        .name = std::move(name),
        .content_type = std::move(content_type),
        .source = package_root / relative_source,
        .destination = destination_root / relative_source,
        .preload = preload,
        .runtime_visible = runtime_visible,
    };

    if (!safe_relative_path(relative_source)) {
        record.error = "source path must be a safe relative path";
        return record;
    }

    if (!path_stays_under_root(package_root, record.source, record.error))
        return record;

    auto ec = std::error_code{};
    if (!fs::is_regular_file(record.source, ec) || ec) {
        record.error = ec ? ec.message() : "source file is missing";
        return record;
    }
    record.present = true;

    fs::create_directories(record.destination.parent_path(), ec);
    if (ec) {
        record.error = ec.message();
        return record;
    }

    ec.clear();
    fs::copy_file(
        record.source,
        record.destination,
        fs::copy_options::overwrite_existing,
        ec);
    if (ec) {
        record.error = ec.message();
        return record;
    }

    record.bytes = file_size_or_zero(record.destination);
    record.copied = true;
    if (auto digest = sha256_file_or_error(record.destination)) {
        record.sha256 = *digest;
    } else {
        record.integrity_error = digest.error();
    }
    return record;
}

auto copy_bundle_absolute_file(fs::path const& package_root,
                               fs::path const& source,
                               fs::path const& output_root,
                               fs::path relative_destination,
                               std::string kind,
                               std::string name,
                               std::string content_type,
                               bool executable = false) -> BundleFileRecord {
    BundleFileRecord record{
        .kind = std::move(kind),
        .name = std::move(name),
        .content_type = std::move(content_type),
        .source = source,
        .destination = output_root / relative_destination,
    };

    if (!safe_relative_path(relative_destination)) {
        record.error = "destination path must be a safe relative path";
        return record;
    }
    if (!path_stays_under_root(package_root, source, record.error))
        return record;
    if (!path_stays_under_root(output_root, record.destination, record.error))
        return record;

    auto ec = std::error_code{};
    if (!fs::is_regular_file(record.source, ec) || ec) {
        record.error = ec ? ec.message() : "source file is missing";
        return record;
    }
    record.present = true;

    fs::create_directories(record.destination.parent_path(), ec);
    if (ec) {
        record.error = ec.message();
        return record;
    }

    ec.clear();
    fs::copy_file(
        record.source,
        record.destination,
        fs::copy_options::overwrite_existing,
        ec);
    if (ec) {
        record.error = ec.message();
        return record;
    }
    if (executable) {
        ec.clear();
        fs::permissions(
            record.destination,
            fs::perms::owner_exec | fs::perms::group_exec
                | fs::perms::others_exec,
            fs::perm_options::add,
            ec);
        if (ec) {
            record.error = ec.message();
            return record;
        }
    }

    record.bytes = file_size_or_zero(record.destination);
    record.copied = true;
    if (auto digest = sha256_file_or_error(record.destination)) {
        record.sha256 = *digest;
    } else {
        record.integrity_error = digest.error();
    }
    return record;
}

auto write_generated_bundle_file(fs::path const& output_root,
                                 fs::path relative_destination,
                                 std::string kind,
                                 std::string name,
                                 std::string content_type,
                                 std::string_view contents,
                                 bool executable = false)
    -> BundleFileRecord {
    BundleFileRecord record{
        .kind = std::move(kind),
        .name = std::move(name),
        .content_type = std::move(content_type),
        .source = fs::path{"<generated>"} / relative_destination,
        .destination = output_root / relative_destination,
    };

    if (!safe_relative_path(relative_destination)) {
        record.error = "destination path must be a safe relative path";
        return record;
    }
    if (!path_stays_under_root(output_root, record.destination, record.error))
        return record;

    if (!write_text_file(record.destination, contents, record.error))
        return record;
    if (executable) {
        auto ec = std::error_code{};
        fs::permissions(
            record.destination,
            fs::perms::owner_exec | fs::perms::group_exec
                | fs::perms::others_exec,
            fs::perm_options::add,
            ec);
        if (ec) {
            record.error = ec.message();
            return record;
        }
    }

    record.present = true;
    record.copied = true;
    record.bytes = file_size_or_zero(record.destination);
    if (auto digest = sha256_file_or_error(record.destination)) {
        record.sha256 = *digest;
    } else {
        record.integrity_error = digest.error();
    }
    return record;
}

auto inspect_bundle_file(fs::path const& bundle_root,
                         std::string kind,
                         std::string name,
                         fs::path relative_source,
                         std::string content_type = {},
                         bool preload = false,
                         bool runtime_visible = false) -> BundleFileRecord {
    BundleFileRecord record{
        .kind = std::move(kind),
        .name = std::move(name),
        .content_type = std::move(content_type),
        .source = bundle_root / relative_source,
        .destination = bundle_root / relative_source,
        .preload = preload,
        .runtime_visible = runtime_visible,
    };

    if (!safe_relative_path(relative_source)) {
        record.error = "source path must be a safe relative path";
        return record;
    }

    if (!path_stays_under_root(bundle_root, record.destination, record.error))
        return record;

    auto ec = std::error_code{};
    if (!fs::is_regular_file(record.destination, ec) || ec) {
        record.error = ec ? ec.message() : "bundle file is missing";
        return record;
    }

    record.present = true;
    record.bytes = file_size_or_zero(record.destination);
    if (auto digest = sha256_file_or_error(record.destination)) {
        record.sha256 = *digest;
    } else {
        record.integrity_error = digest.error();
    }
    return record;
}

auto generated_bundle_file(fs::path const& output_root,
                           fs::path relative_destination,
                           std::string kind,
                           std::string name,
                           std::string content_type) -> BundleFileRecord {
    auto record = inspect_bundle_file(
        output_root,
        std::move(kind),
        std::move(name),
        relative_destination,
        std::move(content_type));
    record.source = fs::path{"<generated>"} / relative_destination;
    return record;
}

void append_expected_package_files(BundleSummary& bundle, bool copy_files) {
    auto append = [&](std::string kind,
                      std::string name,
                      fs::path source,
                      std::string content_type = {},
                      bool preload = false,
                      bool runtime_visible = false) {
        if (copy_files) {
            bundle.files.push_back(copy_bundle_file(
                bundle.package_root,
                bundle.resource_root,
                std::move(kind),
                std::move(name),
                std::move(source),
                std::move(content_type),
                preload,
                runtime_visible));
        } else {
            bundle.files.push_back(inspect_bundle_file(
                bundle.resource_root,
                std::move(kind),
                std::move(name),
                std::move(source),
                std::move(content_type),
                preload,
                runtime_visible));
        }
    };

    append("manifest",
           "phenotype.package.toml",
           "phenotype.package.toml",
           "application/toml");

    for (auto const& asset : bundle.package.catalog.assets) {
        append("asset",
               asset.name,
               asset.source,
               asset.content_type,
               asset.preload,
               asset.runtime_visible);
    }
    for (auto const& locale : bundle.package.catalog.locales) {
        append("locale", locale.tag, locale.source, "application/toml");
    }
    for (auto const& font : bundle.package.catalog.fonts) {
        append("font", font.family, font.source, "application/toml");
    }
    if (!bundle.package.catalog.debug.artifact_manifest.empty()) {
        append("debug",
               "artifact_manifest",
               bundle.package.catalog.debug.artifact_manifest,
               "application/json");
    }
}

auto macos_app_info_plist(PackageSummary const& package,
                          std::string_view executable_name,
                          std::string_view icon_file) -> std::string {
    auto display_name = package.display_name.empty()
        ? package.application_id
        : package.display_name;
    auto icon_block = std::string{};
    if (!icon_file.empty()) {
        icon_block = std::format(
            "  <key>CFBundleIconFile</key>\n"
            "  <string>{}</string>\n",
            xml_escape(icon_file));
    }
    return std::format(
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
        "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "  <key>CFBundleDevelopmentRegion</key>\n"
        "  <string>en</string>\n"
        "  <key>CFBundleDisplayName</key>\n"
        "  <string>{}</string>\n"
        "  <key>CFBundleExecutable</key>\n"
        "  <string>{}</string>\n"
        "{}"
        "  <key>CFBundleIdentifier</key>\n"
        "  <string>{}</string>\n"
        "  <key>CFBundleName</key>\n"
        "  <string>{}</string>\n"
        "  <key>CFBundlePackageType</key>\n"
        "  <string>APPL</string>\n"
        "  <key>CFBundleShortVersionString</key>\n"
        "  <string>{}</string>\n"
        "  <key>CFBundleVersion</key>\n"
        "  <string>{}</string>\n"
        "  <key>LSMinimumSystemVersion</key>\n"
        "  <string>13.0</string>\n"
        "  <key>NSHighResolutionCapable</key>\n"
        "  <true/>\n"
        "  <key>NSSupportsAutomaticGraphicsSwitching</key>\n"
        "  <true/>\n"
        "</dict>\n"
        "</plist>\n",
        xml_escape(display_name),
        xml_escape(executable_name),
        icon_block,
        xml_escape(package.application_id),
        xml_escape(display_name),
        xml_escape(package.version),
        xml_escape(package.version));
}

auto macos_app_launcher_script(std::string_view binary_name) -> std::string {
    return std::format(
        "#!/bin/sh\n"
        "set -eu\n"
        "APP_CONTENTS=$(CDPATH= cd -- \"$(dirname -- \"$0\")/..\" && pwd)\n"
        "export PHENOTYPE_PACKAGE_ROOT=\"$APP_CONTENTS/Resources\"\n"
        "exec \"$APP_CONTENTS/MacOS/{}\" \"$@\"\n",
        binary_name);
}

auto macos_app_icon_file_name(PackageSummary const& package)
    -> std::string {
    auto app_icon = phenotype::find_asset(package.catalog, "app.icon");
    if (!app_icon)
        return {};
    auto const& asset = app_icon->get();
    if (asset.content_type != "image/svg+xml" || !asset.source.ends_with(".svg"))
        return {};
    if (!portable_bundle_file_name(package.entry))
        return {};
    return package.entry + ".icns";
}

auto process_failure_message(std::string_view tool,
                             cppx::process::CapturedProcessResult const& result)
    -> std::string {
    auto tail = !result.stderr_text.empty()
        ? std::string{output_tail(result.stderr_text, 2048)}
        : std::string{output_tail(result.stdout_text, 2048)};
    if (!tail.empty()) {
        return std::format(
            "{} exited with {}: {}",
            tool,
            result.exit_code,
            tail);
    }
    return std::format("{} exited with {}", tool, result.exit_code);
}

auto generate_macos_app_icon(BundleSummary const& bundle,
                             std::string const& icon_file)
    -> BundleFileRecord {
    auto record = BundleFileRecord{
        .kind = "macos_app",
        .name = "app_icon",
        .content_type = "image/icns",
        .destination = bundle.resource_root / icon_file,
    };
    auto app_icon = phenotype::find_asset(bundle.package.catalog, "app.icon");
    if (!app_icon) {
        record.error = "package app.icon asset is missing";
        return record;
    }
    record.source = bundle.package_root / app_icon->get().source;

    if (!path_stays_under_root(bundle.package_root, record.source, record.error))
        return record;
    if (!path_stays_under_root(bundle.resource_root, record.destination, record.error))
        return record;

    auto ec = std::error_code{};
    if (!fs::is_regular_file(record.source, ec) || ec) {
        record.error = ec ? ec.message() : "app icon SVG source is missing";
        return record;
    }
    fs::create_directories(record.destination.parent_path(), ec);
    if (ec) {
        record.error = ec.message();
        return record;
    }

    auto iconset = bundle.resource_root
        / (bundle.package.entry + ".iconset");
    fs::remove_all(iconset, ec);
    ec.clear();
    fs::create_directories(iconset, ec);
    if (ec) {
        record.error = ec.message();
        return record;
    }

    auto const variants = std::array{
        std::pair{"icon_16x16.png", 16},
        std::pair{"icon_16x16@2x.png", 32},
        std::pair{"icon_32x32.png", 32},
        std::pair{"icon_32x32@2x.png", 64},
        std::pair{"icon_128x128.png", 128},
        std::pair{"icon_128x128@2x.png", 256},
        std::pair{"icon_256x256.png", 256},
        std::pair{"icon_256x256@2x.png", 512},
        std::pair{"icon_512x512.png", 512},
        std::pair{"icon_512x512@2x.png", 1024},
    };
    for (auto const& [name, size] : variants) {
        auto output = iconset / name;
        auto result = cppx::process::system::capture({
            .program = "sips",
            .args = {
                "-s",
                "format",
                "png",
                "-z",
                std::to_string(size),
                std::to_string(size),
                path_string(fs::absolute(record.source)),
                "--out",
                path_string(fs::absolute(output)),
            },
            .cwd = bundle.package_root,
            .timeout = std::chrono::seconds{30},
        });
        if (!result) {
            record.error = std::format(
                "failed to run sips: {}",
                cppx::process::to_string(result.error()));
            fs::remove_all(iconset, ec);
            return record;
        }
        if (result->timed_out || result->exit_code != 0) {
            record.error = result->timed_out
                ? "sips timed out while generating app icon PNGs"
                : process_failure_message("sips", *result);
            fs::remove_all(iconset, ec);
            return record;
        }
    }

    auto iconutil = cppx::process::system::capture({
        .program = "iconutil",
        .args = {
            "--convert",
            "icns",
            "--output",
            path_string(fs::absolute(record.destination)),
            path_string(fs::absolute(iconset)),
        },
        .cwd = bundle.package_root,
        .timeout = std::chrono::seconds{30},
    });
    fs::remove_all(iconset, ec);
    if (!iconutil) {
        record.error = std::format(
            "failed to run iconutil: {}",
            cppx::process::to_string(iconutil.error()));
        return record;
    }
    if (iconutil->timed_out || iconutil->exit_code != 0) {
        record.error = iconutil->timed_out
            ? "iconutil timed out while generating app icon"
            : process_failure_message("iconutil", *iconutil);
        return record;
    }

    record.present = path_exists(record.destination);
    record.copied = record.present;
    record.bytes = file_size_or_zero(record.destination);
    if (!record.present || record.bytes == 0) {
        record.error = "iconutil did not produce a non-empty .icns file";
        return record;
    }
    if (auto digest = sha256_file_or_error(record.destination)) {
        record.sha256 = *digest;
    } else {
        record.integrity_error = digest.error();
    }
    return record;
}

void append_macos_app_files(BundleSummary& bundle, bool copy_files) {
    auto executable_name = bundle.package.entry;
    if (!portable_bundle_file_name(executable_name)) {
        bundle.files.push_back({
            .kind = "macos_app",
            .name = "launcher",
            .content_type = "application/x-sh",
            .source = fs::path{"<generated>"} / "Contents" / "MacOS"
                / executable_name,
            .destination = bundle.output_root / "Contents" / "MacOS"
                / executable_name,
            .error = "application entry must be a portable bundle executable name",
        });
        return;
    }

    auto binary_name = executable_name + ".bin";
    auto info_plist = fs::path{"Contents"} / "Info.plist";
    auto pkg_info = fs::path{"Contents"} / "PkgInfo";
    auto launcher = fs::path{"Contents"} / "MacOS" / executable_name;
    auto binary = fs::path{"Contents"} / "MacOS" / binary_name;
    auto icon_file = macos_app_icon_file_name(bundle.package);

    if (copy_files) {
        if (!icon_file.empty()) {
            bundle.files.push_back(generate_macos_app_icon(bundle, icon_file));
        }
        bundle.files.push_back(write_generated_bundle_file(
            bundle.output_root,
            info_plist,
            "macos_app",
            "Info.plist",
            "application/xml",
            macos_app_info_plist(bundle.package, executable_name, icon_file)));
        bundle.files.push_back(write_generated_bundle_file(
            bundle.output_root,
            pkg_info,
            "macos_app",
            "PkgInfo",
            "text/plain",
            "APPL????\n"));
        bundle.files.push_back(write_generated_bundle_file(
            bundle.output_root,
            launcher,
            "macos_app",
            "launcher",
            "application/x-sh",
            macos_app_launcher_script(binary_name),
            true));
        bundle.files.push_back(copy_bundle_absolute_file(
            bundle.package_root,
            bundle.package_root / ".exon" / "debug"
                / executable_filename(bundle.package.entry),
            bundle.output_root,
            binary,
            "macos_app",
            "executable",
            "application/octet-stream",
            true));
    } else {
        if (!icon_file.empty()) {
            bundle.files.push_back(inspect_bundle_file(
                bundle.resource_root,
                "macos_app",
                "app_icon",
                icon_file,
                "image/icns"));
        }
        bundle.files.push_back(generated_bundle_file(
            bundle.output_root,
            info_plist,
            "macos_app",
            "Info.plist",
            "application/xml"));
        bundle.files.push_back(generated_bundle_file(
            bundle.output_root,
            pkg_info,
            "macos_app",
            "PkgInfo",
            "text/plain"));
        bundle.files.push_back(generated_bundle_file(
            bundle.output_root,
            launcher,
            "macos_app",
            "launcher",
            "application/x-sh"));
        bundle.files.push_back(inspect_bundle_file(
            bundle.output_root,
            "macos_app",
            "executable",
            binary,
            "application/octet-stream"));
    }
}

bool write_bundle_manifest(BundleSummary& bundle) {
    bundle.bundle_manifest_present = true;
    for (int attempt = 0; attempt < 4; ++attempt) {
        auto manifest = bundle_json(bundle);
        if (!write_text_file(bundle.manifest_path, manifest, bundle.error)) {
            bundle.bundle_manifest_present = false;
            return false;
        }
        auto bytes = file_size_or_zero(bundle.manifest_path);
        if (bytes == bundle.bundle_manifest_bytes)
            return true;
        bundle.bundle_manifest_bytes = bytes;
    }
    return true;
}

auto normalized_bundle_format(std::string_view value)
    -> std::expected<std::string, std::string> {
    auto format = std::string{value.empty() ? "resources" : value};
    std::ranges::transform(format, format.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    if (format == "resources" || format == "macos-app")
        return format;
    return std::unexpected{"package bundle --format must be 'resources' or 'macos-app'"};
}

auto detect_bundle_format(fs::path const& bundle_root)
    -> std::pair<std::string, fs::path> {
    if (path_exists(bundle_root / "phenotype.package.toml"))
        return {"resources", bundle_root};
    auto app_resources = bundle_root / "Contents" / "Resources";
    if (path_exists(app_resources / "phenotype.package.toml"))
        return {"macos-app", app_resources};
    return {"resources", bundle_root};
}

auto build_package_bundle(fs::path package_root,
                          fs::path output_root,
                          std::string format) -> BundleSummary {
    BundleSummary bundle{
        .format = std::move(format),
        .package_root = std::move(package_root),
        .output_root = std::move(output_root),
    };
    bundle.resource_root = bundle.format == "macos-app"
        ? bundle.output_root / "Contents" / "Resources"
        : bundle.output_root;
    bundle.package = package_summary(bundle.package_root);
    bundle.checks = package_checks(bundle.package);
    if (!all_ok(bundle.checks)) {
        bundle.error = "package inspect checks failed";
        return bundle;
    }
    if (bundle.format == "macos-app"
        && bundle.output_root.extension() != ".app") {
        bundle.error = "macos-app format requires an output directory ending in .app";
        return bundle;
    }

    auto ec = std::error_code{};
    fs::create_directories(bundle.output_root, ec);
    if (ec) {
        bundle.error = ec.message();
        return bundle;
    }

    append_expected_package_files(bundle, true);
    if (bundle.format == "macos-app")
        append_macos_app_files(bundle, true);
    bundle.total_bytes = bundle_total_bytes(bundle.files);
    bundle.verified_file_count = bundle_verified_file_count(bundle.files);

    if (!bundle_files_integrity_ok(bundle.files, true)) {
        bundle.error = "one or more package resources failed to copy or verify";
        return bundle;
    }

    bundle.manifest_path = bundle.resource_root / "phenotype.bundle.json";
    bundle.ok = true;
    if (!write_bundle_manifest(bundle)) {
        bundle.ok = false;
        return bundle;
    }
    compare_stored_bundle_manifest(bundle);
    bundle.checks.push_back(stored_bundle_manifest_check(bundle));
    if (!stored_bundle_manifest_ok(bundle)) {
        bundle.ok = false;
        bundle.error = "staged bundle manifest did not match copied resources";
    }
    return bundle;
}

auto verify_package_bundle(fs::path bundle_root) -> BundleSummary {
    auto [format, resource_root] = detect_bundle_format(bundle_root);
    BundleSummary bundle{
        .command = "package verify-bundle",
        .format = std::move(format),
        .package_root = resource_root,
        .output_root = std::move(bundle_root),
        .resource_root = std::move(resource_root),
    };
    bundle.manifest_path = bundle.resource_root / "phenotype.bundle.json";
    bundle.package = package_summary(bundle.resource_root);
    bundle.checks = package_checks(bundle.package);
    bundle.bundle_manifest_present = path_exists(bundle.manifest_path);
    bundle.bundle_manifest_bytes = file_size_or_zero(bundle.manifest_path);
    bundle.checks.push_back({
        .name = "bundle_manifest",
        .ok = bundle.bundle_manifest_present && bundle.bundle_manifest_bytes > 0,
        .detail = std::format("phenotype.bundle.json ({} bytes)",
                              bundle.bundle_manifest_bytes),
        .hint = "Run phenotype package bundle before verifying a bundle directory.",
    });

    if (!all_ok(bundle.checks)) {
        bundle.error = "bundle package checks failed";
        return bundle;
    }

    append_expected_package_files(bundle, false);
    if (bundle.format == "macos-app")
        append_macos_app_files(bundle, false);
    bundle.total_bytes = bundle_total_bytes(bundle.files);
    bundle.verified_file_count = bundle_verified_file_count(bundle.files);
    compare_stored_bundle_manifest(bundle);
    bundle.checks.push_back(stored_bundle_manifest_check(bundle));

    if (!bundle_files_integrity_ok(bundle.files, false)) {
        bundle.error = "one or more staged package resources failed integrity validation";
        return bundle;
    }
    if (!stored_bundle_manifest_ok(bundle)) {
        bundle.error = "staged bundle manifest did not match copied resources";
        return bundle;
    }

    bundle.ok = true;
    return bundle;
}

export int run_package_inspect(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "package inspect");
    if (!path)
        return print_error("package inspect", path.error(), invocation.has("json"));

    auto summary = package_summary(*path);
    auto checks = package_checks(summary);
    if (invocation.has("json")) {
        std::println("{}", package_json(summary, checks));
    } else {
        print_checks("phenotype package inspect", checks);
    }
    return all_ok(checks) ? 0 : 1;
}

export int run_package_list(cppx::cli::Invocation const& invocation) {
    auto root = optional_positional_or_error(invocation, "package list", ".");
    if (!root)
        return print_error("package list", root.error(), invocation.has("json"));

    auto manifests = package_manifest_roots(*root);
    auto entries = std::vector<std::pair<PackageSummary, std::vector<Check>>>{};
    entries.reserve(manifests.size());
    auto ok = path_is_directory(*root) && !manifests.empty();
    for (auto const& manifest_root : manifests) {
        auto summary = package_summary(manifest_root);
        auto checks = package_checks(summary);
        ok = ok && all_ok(checks);
        entries.push_back({std::move(summary), std::move(checks)});
    }

    if (invocation.has("json")) {
        auto packages = std::string{"["};
        for (std::size_t i = 0; i < entries.size(); ++i) {
            if (i > 0)
                packages += ",";
            packages += package_entry_json(entries[i].first, entries[i].second);
        }
        packages += "]";
        std::println(
            "{{\"schema_version\":1,\"command\":\"package list\","
            "\"ok\":{},\"root\":{},\"package_count\":{},\"packages\":{}}}",
            ok ? "true" : "false",
            json_string(path_string(*root)),
            entries.size(),
            packages);
    } else {
        auto lines = std::vector<cppx::terminal::StatusLine>{};
        lines.reserve(entries.size());
        for (auto const& [summary, checks] : entries) {
            lines.push_back({
                .label = summary.application_id.empty()
                    ? path_string(summary.root)
                    : summary.application_id,
                .value = path_string(summary.root),
                .status = all_ok(checks) ? cppx::terminal::StatusKind::ok
                                         : cppx::terminal::StatusKind::fail,
            });
        }
        if (lines.empty()) {
            lines.push_back({
                .label = "packages",
                .value = "none",
                .status = cppx::terminal::StatusKind::fail,
            });
        }
        std::println("phenotype package list");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
    }
    return ok ? 0 : 1;
}

export int run_package_bundle(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "package bundle");
    if (!path)
        return print_error("package bundle", path.error(), invocation.has("json"));
    auto output = invocation.value("output");
    if (!output) {
        return print_error(
            "package bundle",
            "package bundle requires --output <dir>",
            invocation.has("json"));
    }
    auto format = normalized_bundle_format(
        invocation.value("format").value_or("resources"));
    if (!format) {
        return print_error(
            "package bundle",
            format.error(),
            invocation.has("json"));
    }

    auto bundle = build_package_bundle(
        fs::path{*path},
        fs::path{std::string{*output}},
        *format);
    if (invocation.has("json")) {
        std::println("{}", bundle_json(bundle));
    } else {
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "format",
             .value = bundle.format,
             .status = cppx::terminal::StatusKind::ok},
            {.label = "package",
             .value = path_string(bundle.package_root),
             .status = all_ok(bundle.checks)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
            {.label = "output",
             .value = path_string(bundle.output_root),
             .status = bundle.ok ? cppx::terminal::StatusKind::ok
                                 : cppx::terminal::StatusKind::fail},
            {.label = "resources",
             .value = path_string(bundle.resource_root),
             .status = bundle.ok ? cppx::terminal::StatusKind::ok
                                 : cppx::terminal::StatusKind::skip},
            {.label = "manifest",
             .value = path_string(bundle.manifest_path),
             .status = bundle.ok ? cppx::terminal::StatusKind::ok
                                 : cppx::terminal::StatusKind::skip},
            {.label = "files",
             .value = std::format("{}", bundle.files.size()),
             .status = bundle_files_integrity_ok(bundle.files, true)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
        };
        std::println("phenotype package bundle");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        if (!bundle.error.empty()) {
            std::println("{}",
                         cppx::terminal::format_diagnostic({
                             .severity = cppx::terminal::DiagnosticSeverity::error,
                             .message = bundle.error,
                             .context = "package bundle",
                         }));
        }
    }
    return bundle.ok ? 0 : 1;
}

export int run_package_verify_bundle(cppx::cli::Invocation const& invocation) {
    auto path = first_positional_or_error(invocation, "package verify-bundle");
    if (!path) {
        return print_error(
            "package verify-bundle",
            path.error(),
            invocation.has("json"));
    }

    auto bundle = verify_package_bundle(fs::path{*path});
    if (invocation.has("json")) {
        std::println("{}", bundle_json(bundle));
    } else {
        auto lines = std::vector<cppx::terminal::StatusLine>{
            {.label = "format",
             .value = bundle.format,
             .status = cppx::terminal::StatusKind::ok},
            {.label = "bundle",
             .value = path_string(bundle.output_root),
             .status = all_ok(bundle.checks)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
            {.label = "resources",
             .value = path_string(bundle.resource_root),
             .status = path_is_directory(bundle.resource_root)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
            {.label = "manifest",
             .value = path_string(bundle.manifest_path),
             .status = bundle.bundle_manifest_present
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
            {.label = "integrity",
             .value = std::format("{}/{} files, {} bytes",
                                  bundle.verified_file_count,
                                  bundle.files.size(),
                                  bundle.total_bytes),
             .status = bundle_files_integrity_ok(bundle.files, false)
                ? cppx::terminal::StatusKind::ok
                : cppx::terminal::StatusKind::fail},
        };
        std::println("phenotype package verify-bundle");
        std::println("{}", cppx::terminal::format_status_frame(lines, false));
        if (!bundle.error.empty()) {
            std::println("{}",
                         cppx::terminal::format_diagnostic({
                             .severity = cppx::terminal::DiagnosticSeverity::error,
                             .message = bundle.error,
                             .context = "package verify-bundle",
                         }));
        }
    }
    return bundle.ok ? 0 : 1;
}

}

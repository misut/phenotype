inline json::Value explorer_theme_system_debug_json(
        ExplorerChromeMetrics const& chrome) {
    json::Object out;
    out.emplace(
        "contract_version",
        json::Value{static_cast<std::int64_t>(
            chrome.theme_contract_version)});
    out.emplace("profile_name", json::Value{chrome.theme_profile_name});
    out.emplace("reference", json::Value{chrome.theme_reference});
    out.emplace("font_policy", json::Value{chrome.theme_font_policy});
    out.emplace("material_policy", json::Value{chrome.theme_material_policy});
    out.emplace(
        "iconography_policy",
        json::Value{chrome.theme_iconography_policy});
    out.emplace(
        "icon_asset_policy",
        json::Value{chrome.theme_icon_asset_policy});
    out.emplace("usage_policy", json::Value{chrome.theme_usage_policy});
    out.emplace("container_policy", json::Value{chrome.theme_container_policy});
    out.emplace(
        "performance_policy",
        json::Value{chrome.theme_performance_policy});
    out.emplace(
        "accessibility_policy",
        json::Value{chrome.theme_accessibility_policy});
    out.emplace("fallback_policy", json::Value{chrome.theme_fallback_policy});
    return json::Value{std::move(out)};
}

inline auto resource_contract_locale_keys(
        phenotype::ResourceCatalog const& catalog)
        -> std::vector<std::string_view> {
    auto out = std::vector<std::string_view>{};
    if (auto locale = phenotype::find_locale(catalog, catalog.default_locale)) {
        out.reserve(locale->get().strings.size());
        for (auto const& text : locale->get().strings)
            out.push_back(text.key);
    }
    return out;
}

inline json::Value string_vector_debug_json(
        std::span<std::string const> values) {
    json::Array out;
    for (auto const& value : values)
        out.push_back(json::Value{value});
    return json::Value{std::move(out)};
}

inline json::Value locale_coverage_debug_json(
        std::span<phenotype::LocaleCoverage const> coverage) {
    json::Array out;
    for (auto const& locale : coverage) {
        json::Object item;
        item.emplace("tag", json::Value{locale.tag});
        item.emplace("default_locale", json::Value{locale.default_locale});
        item.emplace(
            "fallback_chain",
            string_vector_debug_json(locale.fallback_chain));
        item.emplace(
            "declared_string_count",
            json::Value{static_cast<std::int64_t>(
                locale.declared_string_count)});
        item.emplace(
            "required_key_count",
            json::Value{static_cast<std::int64_t>(
                locale.required_key_count)});
        item.emplace(
            "resolved_key_count",
            json::Value{static_cast<std::int64_t>(
                locale.resolved_key_count)});
        item.emplace(
            "missing_keys",
            string_vector_debug_json(locale.missing_keys));
        out.push_back(json::Value{std::move(item)});
    }
    return json::Value{std::move(out)};
}

struct FileTypeIconAsset {
    std::string_view name;
    std::string_view source;
};

inline constexpr FileTypeIconAsset k_file_type_icon_assets[] = {
    {"file_type.folder.icon", "assets/icons/file-types/folder.svg"},
    {"file_type.document.icon", "assets/icons/file-types/document.svg"},
    {"file_type.pdf.icon", "assets/icons/file-types/pdf.svg"},
    {"file_type.text.icon", "assets/icons/file-types/text.svg"},
    {"file_type.image.icon", "assets/icons/file-types/image.svg"},
    {"file_type.movie.icon", "assets/icons/file-types/movie.svg"},
    {"file_type.archive.icon", "assets/icons/file-types/archive.svg"},
    {"file_type.audio.icon", "assets/icons/file-types/audio.svg"},
    {"file_type.code.icon", "assets/icons/file-types/code.svg"},
    {"file_type.spreadsheet.icon",
     "assets/icons/file-types/spreadsheet.svg"},
    {"file_type.presentation.icon",
     "assets/icons/file-types/presentation.svg"},
};

inline bool is_file_type_icon_asset_name(std::string_view name) noexcept {
    return name.starts_with("file_type.") && name.ends_with(".icon");
}

inline json::Value file_type_icon_source_map_debug_json(
        phenotype::ResourceCatalog const& catalog) {
    json::Object out;
    for (auto const& item : file_type_symbol_contract()) {
        auto asset_name = file_type_icon_asset_name_for_symbol(item.symbol);
        auto asset_source = file_type_icon_asset_source_for_symbol(item.symbol);
        json::Object entry;
        entry.emplace(
            "symbol",
            json::Value{std::string{icon_catalog::name(item.symbol)}});
        entry.emplace(
            "semantic_reference_name",
            json::Value{std::string{
                icon_catalog::semantic_reference_name(item.symbol)}});
        entry.emplace("package_asset_name", json::Value{asset_name});
        entry.emplace("package_asset_source", json::Value{asset_source});
        entry.emplace(
            "package_asset_policy",
            json::Value{std::string{file_type_icon_asset_policy()}});
        if (auto asset = phenotype::find_asset(catalog, asset_name)) {
            entry.emplace("package_declared", json::Value{true});
            entry.emplace(
                "package_content_type",
                json::Value{asset->get().content_type});
            entry.emplace(
                "package_runtime_visible",
                json::Value{asset->get().runtime_visible});
            entry.emplace(
                "package_preload",
                json::Value{asset->get().preload});
        } else {
            entry.emplace("package_declared", json::Value{false});
            entry.emplace("package_content_type", json::Value{""});
            entry.emplace("package_runtime_visible", json::Value{false});
            entry.emplace("package_preload", json::Value{false});
        }
        entry.emplace(
            "source_attribution",
            icon_source_attribution_debug_json(item.symbol));
        out.emplace(std::string{item.token}, json::Value{std::move(entry)});
    }
    return json::Value{std::move(out)};
}

inline json::Value file_explorer_resource_system_debug_json(
        std::string_view profile) {
    auto catalog = file_explorer_resource_catalog(profile);
    auto required_keys = resource_contract_locale_keys(catalog);
    auto contract = phenotype::resource_catalog_contract(
        catalog,
        std::span<std::string_view const>{required_keys});

    json::Array platforms;
    for (auto const& platform : catalog.application.platforms)
        platforms.push_back(json::Value{platform});

    json::Object application;
    application.emplace("id", json::Value{catalog.application.id});
    application.emplace(
        "display_name",
        json::Value{catalog.application.display_name});
    application.emplace("version", json::Value{catalog.application.version});
    application.emplace("entry", json::Value{catalog.application.entry});
    application.emplace("platforms", json::Value{std::move(platforms)});

    json::Object app_icon;
    app_icon.emplace("declared", json::Value{contract.app_icon_declared});
    app_icon.emplace("svg", json::Value{contract.app_icon_svg});
    app_icon.emplace("preload", json::Value{contract.app_icon_preload});
    if (auto asset = phenotype::find_asset(catalog, "app.icon")) {
        app_icon.emplace("source", json::Value{asset->get().source});
        app_icon.emplace(
            "content_type",
            json::Value{asset->get().content_type});
    }

    json::Array file_type_icon_assets;
    for (auto const& asset : catalog.assets) {
        if (!is_file_type_icon_asset_name(asset.name))
            continue;
        json::Object icon_asset;
        icon_asset.emplace("name", json::Value{asset.name});
        icon_asset.emplace("source", json::Value{asset.source});
        icon_asset.emplace("content_type", json::Value{asset.content_type});
        icon_asset.emplace("preload", json::Value{asset.preload});
        icon_asset.emplace(
            "runtime_visible",
            json::Value{asset.runtime_visible});
        file_type_icon_assets.push_back(json::Value{std::move(icon_asset)});
    }

    json::Object defaults;
    defaults.emplace("locale", json::Value{catalog.default_locale});
    defaults.emplace(
        "default_locale_declared",
        json::Value{contract.default_locale_declared});
    defaults.emplace("font_family", json::Value{catalog.default_font_family});
    defaults.emplace(
        "default_font_declared",
        json::Value{contract.default_font_declared});
    defaults.emplace(
        "default_font_has_cjk_fallback",
        json::Value{contract.default_font_has_cjk_fallback});

    json::Object locales;
    locales.emplace(
        "count",
        json::Value{static_cast<std::int64_t>(contract.locale_count)});
    locales.emplace(
        "string_count",
        json::Value{static_cast<std::int64_t>(contract.locale_string_count)});
    locales.emplace(
        "required_key_count",
        json::Value{static_cast<std::int64_t>(required_keys.size())});
    locales.emplace(
        "coverage",
        locale_coverage_debug_json(contract.locale_coverage));

    json::Object fonts;
    fonts.emplace(
        "count",
        json::Value{static_cast<std::int64_t>(contract.font_count)});
    fonts.emplace(
        "registered_count",
        json::Value{static_cast<std::int64_t>(
            contract.registered_font_count)});

    json::Object debug;
    debug.emplace(
        "artifact_manifest",
        json::Value{catalog.debug.artifact_manifest});
    debug.emplace("probe_scene", json::Value{catalog.debug.probe_scene});
    debug.emplace("verifier", json::Value{catalog.debug.verifier});
    debug.emplace(
        "artifact_manifest_declared",
        json::Value{contract.debug_artifact_manifest_declared});
    debug.emplace(
        "probe_scene_declared",
        json::Value{contract.debug_probe_scene_declared});
    debug.emplace(
        "verifier_declared",
        json::Value{contract.debug_verifier_declared});

    json::Object out;
    out.emplace("schema_version", json::Value{1});
    out.emplace("application", json::Value{std::move(application)});
    out.emplace(
        "asset_count",
        json::Value{static_cast<std::int64_t>(contract.asset_count)});
    out.emplace(
        "preload_asset_count",
        json::Value{static_cast<std::int64_t>(
            contract.preload_asset_count)});
    out.emplace(
        "runtime_visible_asset_count",
        json::Value{static_cast<std::int64_t>(
            contract.runtime_visible_asset_count)});
    out.emplace(
        "svg_asset_count",
        json::Value{static_cast<std::int64_t>(contract.svg_asset_count)});
    out.emplace(
        "preload_svg_asset_count",
        json::Value{static_cast<std::int64_t>(
            contract.preload_svg_asset_count)});
    out.emplace(
        "runtime_visible_svg_asset_count",
        json::Value{static_cast<std::int64_t>(
            contract.runtime_visible_svg_asset_count)});
    out.emplace(
        "file_type_icon_asset_count",
        json::Value{static_cast<std::int64_t>(
            file_type_icon_assets.size())});
    out.emplace(
        "file_type_icon_source_family",
        json::Value{std::string{file_type_icon_source_family()}});
    out.emplace(
        "file_type_icon_source_revision",
        json::Value{std::string{file_type_icon_source_revision()}});
    out.emplace(
        "file_type_icon_license_asset",
        json::Value{std::string{file_type_icon_license_asset_source()}});
    out.emplace(
        "file_type_icon_assets",
        json::Value{std::move(file_type_icon_assets)});
    out.emplace(
        "file_type_icon_source_map",
        file_type_icon_source_map_debug_json(catalog));
    out.emplace(
        "svg_asset_policy",
        json::Value{std::string{phenotype::svg_asset_contract_policy()}});
    out.emplace("app_icon", json::Value{std::move(app_icon)});
    out.emplace("defaults", json::Value{std::move(defaults)});
    out.emplace("locales", json::Value{std::move(locales)});
    out.emplace("fonts", json::Value{std::move(fonts)});
    out.emplace("debug", json::Value{std::move(debug)});
    return json::Value{std::move(out)};
}


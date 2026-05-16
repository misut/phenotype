module;
#include <algorithm>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module phenotype.resources;

import phenotype.types;

export namespace phenotype {

enum class ResourceDiagnosticSeverity {
    Info,
    Warning,
    Error,
};

inline char const* resource_diagnostic_severity_name(
        ResourceDiagnosticSeverity severity) noexcept {
    switch (severity) {
        case ResourceDiagnosticSeverity::Info:    return "info";
        case ResourceDiagnosticSeverity::Warning: return "warning";
        case ResourceDiagnosticSeverity::Error:   return "error";
    }
    return "error";
}

enum class ResourceDiagnosticKind {
    MissingApplicationId,
    MissingDisplayName,
    MissingEntry,
    MissingDefaultLocale,
    MissingDefaultFont,
    MissingAssetName,
    MissingAssetSource,
    DuplicateAssetName,
    MissingLocaleTag,
    MissingLocaleSource,
    DuplicateLocaleTag,
    MissingLocaleKey,
    MissingFontFamily,
    MissingFontSource,
    DuplicateFontFamily,
    MissingArtifactManifest,
    MissingDebugVerifier,
};

inline char const* resource_diagnostic_kind_name(
        ResourceDiagnosticKind kind) noexcept {
    switch (kind) {
        case ResourceDiagnosticKind::MissingApplicationId:
            return "missing_application_id";
        case ResourceDiagnosticKind::MissingDisplayName:
            return "missing_display_name";
        case ResourceDiagnosticKind::MissingEntry:
            return "missing_entry";
        case ResourceDiagnosticKind::MissingDefaultLocale:
            return "missing_default_locale";
        case ResourceDiagnosticKind::MissingDefaultFont:
            return "missing_default_font";
        case ResourceDiagnosticKind::MissingAssetName:
            return "missing_asset_name";
        case ResourceDiagnosticKind::MissingAssetSource:
            return "missing_asset_source";
        case ResourceDiagnosticKind::DuplicateAssetName:
            return "duplicate_asset_name";
        case ResourceDiagnosticKind::MissingLocaleTag:
            return "missing_locale_tag";
        case ResourceDiagnosticKind::MissingLocaleSource:
            return "missing_locale_source";
        case ResourceDiagnosticKind::DuplicateLocaleTag:
            return "duplicate_locale_tag";
        case ResourceDiagnosticKind::MissingLocaleKey:
            return "missing_locale_key";
        case ResourceDiagnosticKind::MissingFontFamily:
            return "missing_font_family";
        case ResourceDiagnosticKind::MissingFontSource:
            return "missing_font_source";
        case ResourceDiagnosticKind::DuplicateFontFamily:
            return "duplicate_font_family";
        case ResourceDiagnosticKind::MissingArtifactManifest:
            return "missing_artifact_manifest";
        case ResourceDiagnosticKind::MissingDebugVerifier:
            return "missing_debug_verifier";
    }
    return "missing_application_id";
}

struct ResourceDiagnostic {
    ResourceDiagnosticSeverity severity = ResourceDiagnosticSeverity::Error;
    ResourceDiagnosticKind kind = ResourceDiagnosticKind::MissingApplicationId;
    std::string path;
    std::string message;
    std::string expected;
    std::string actual;
};

struct ApplicationDescriptor {
    std::string id;
    std::string display_name;
    std::string version;
    std::string entry;
    std::vector<std::string> platforms;
};

struct AssetDescriptor {
    std::string name;
    std::string source;
    std::string content_type;
    bool preload = false;
    bool runtime_visible = false;
};

struct LocaleString {
    std::string key;
    std::string value;
};

struct LocaleDescriptor {
    std::string tag;
    std::string source;
    std::vector<std::string> fallback;
    std::vector<LocaleString> strings;
};

struct FontDescriptor {
    std::string family;
    std::string source;
    bool register_font = false;
    std::vector<std::string> fallback;
};

struct DebugResourceDescriptor {
    std::string artifact_manifest;
    std::string probe_scene;
    std::string verifier;
};

struct ResourceCatalog {
    ApplicationDescriptor application;
    std::string default_locale = "en";
    std::string default_font_family = "Pretendard";
    std::vector<AssetDescriptor> assets;
    std::vector<LocaleDescriptor> locales;
    std::vector<FontDescriptor> fonts;
    DebugResourceDescriptor debug;
};

struct LocalizedString {
    std::string tag;
    std::string key;
    std::string value;
    bool fallback = false;
};

inline bool same_resource_key(
        std::string_view lhs,
        std::string_view rhs) noexcept {
    return lhs == rhs;
}

inline auto find_asset(ResourceCatalog const& catalog, std::string_view name)
    -> std::optional<std::reference_wrapper<AssetDescriptor const>> {
    auto found = std::ranges::find_if(
        catalog.assets,
        [&](AssetDescriptor const& asset) {
            return same_resource_key(asset.name, name);
        });
    if (found == catalog.assets.end())
        return std::nullopt;
    return std::cref(*found);
}

inline auto find_locale(ResourceCatalog const& catalog, std::string_view tag)
    -> std::optional<std::reference_wrapper<LocaleDescriptor const>> {
    auto found = std::ranges::find_if(
        catalog.locales,
        [&](LocaleDescriptor const& locale) {
            return same_resource_key(locale.tag, tag);
        });
    if (found == catalog.locales.end())
        return std::nullopt;
    return std::cref(*found);
}

inline auto find_font(ResourceCatalog const& catalog, std::string_view family)
    -> std::optional<std::reference_wrapper<FontDescriptor const>> {
    auto found = std::ranges::find_if(
        catalog.fonts,
        [&](FontDescriptor const& font) {
            return same_resource_key(font.family, family);
        });
    if (found == catalog.fonts.end())
        return std::nullopt;
    return std::cref(*found);
}

inline auto find_locale_string(
        LocaleDescriptor const& locale,
        std::string_view key) -> std::optional<std::string_view> {
    auto found = std::ranges::find_if(
        locale.strings,
        [&](LocaleString const& text) {
            return same_resource_key(text.key, key);
        });
    if (found == locale.strings.end())
        return std::nullopt;
    return std::string_view{found->value};
}

inline bool contains_text(
        std::span<std::string const> values,
        std::string_view value) {
    return std::ranges::any_of(values, [&](std::string const& current) {
        return same_resource_key(current, value);
    });
}

inline void append_locale_chain(
        ResourceCatalog const& catalog,
        std::string_view tag,
        std::vector<std::string>& out) {
    if (tag.empty() || contains_text(out, tag))
        return;
    auto locale = find_locale(catalog, tag);
    if (!locale)
        return;
    out.push_back(std::string{tag});
    for (auto const& fallback : locale->get().fallback) {
        append_locale_chain(catalog, fallback, out);
    }
}

inline auto locale_fallback_chain(
        ResourceCatalog const& catalog,
        std::string_view requested_tag) -> std::vector<std::string> {
    auto chain = std::vector<std::string>{};
    append_locale_chain(catalog, requested_tag, chain);
    append_locale_chain(catalog, catalog.default_locale, chain);
    return chain;
}

inline auto localized_string(
        ResourceCatalog const& catalog,
        std::string_view key,
        std::string_view requested_tag = {}) -> std::optional<LocalizedString> {
    auto tag = requested_tag.empty()
        ? std::string_view{catalog.default_locale}
        : requested_tag;
    auto chain = locale_fallback_chain(catalog, tag);
    for (auto const& candidate : chain) {
        auto locale = find_locale(catalog, candidate);
        if (!locale)
            continue;
        if (auto value = find_locale_string(locale->get(), key)) {
            return LocalizedString{
                candidate,
                std::string{key},
                std::string{*value},
                !same_resource_key(candidate, tag)};
        }
    }
    return std::nullopt;
}

inline auto theme_with_resource_defaults(
        Theme theme,
        ResourceCatalog const& catalog) -> Theme {
    if (!catalog.default_font_family.empty())
        theme.default_font_family = catalog.default_font_family;
    return theme;
}

inline void add_resource_diagnostic(
        std::vector<ResourceDiagnostic>& diagnostics,
        ResourceDiagnosticKind kind,
        std::string path,
        std::string message,
        std::string expected = {},
        std::string actual = {}) {
    diagnostics.push_back(ResourceDiagnostic{
        .severity = ResourceDiagnosticSeverity::Error,
        .kind = kind,
        .path = std::move(path),
        .message = std::move(message),
        .expected = std::move(expected),
        .actual = std::move(actual),
    });
}

template <typename Range, typename Name>
inline bool resource_name_seen(
        Range const& range,
        std::size_t index,
        Name name_of) {
    auto const name = name_of(range[index]);
    if (name.empty())
        return false;
    for (std::size_t i = 0; i < index; ++i) {
        if (same_resource_key(name_of(range[i]), name))
            return true;
    }
    return false;
}

inline auto validate_resource_catalog(
        ResourceCatalog const& catalog,
        std::span<std::string_view const> required_locale_keys = {})
    -> std::vector<ResourceDiagnostic> {
    auto diagnostics = std::vector<ResourceDiagnostic>{};

    if (catalog.application.id.empty()) {
        add_resource_diagnostic(
            diagnostics,
            ResourceDiagnosticKind::MissingApplicationId,
            "application.id",
            "application id is required",
            "non-empty string");
    }
    if (catalog.application.display_name.empty()) {
        add_resource_diagnostic(
            diagnostics,
            ResourceDiagnosticKind::MissingDisplayName,
            "application.display_name",
            "application display_name is required",
            "non-empty string");
    }
    if (catalog.application.entry.empty()) {
        add_resource_diagnostic(
            diagnostics,
            ResourceDiagnosticKind::MissingEntry,
            "application.entry",
            "application entry is required",
            "non-empty string");
    }
    if (catalog.default_locale.empty() || !find_locale(catalog, catalog.default_locale)) {
        add_resource_diagnostic(
            diagnostics,
            ResourceDiagnosticKind::MissingDefaultLocale,
            "resources.default_locale",
            "default locale must reference a locale descriptor",
            "declared locale tag",
            catalog.default_locale);
    }
    if (catalog.default_font_family.empty()
        || !find_font(catalog, catalog.default_font_family)) {
        add_resource_diagnostic(
            diagnostics,
            ResourceDiagnosticKind::MissingDefaultFont,
            "resources.default_font_family",
            "default font family must reference a font descriptor",
            "declared font family",
            catalog.default_font_family);
    }

    for (std::size_t i = 0; i < catalog.assets.size(); ++i) {
        auto const& asset = catalog.assets[i];
        auto base = std::string{"assets["} + std::to_string(i) + "]";
        if (asset.name.empty()) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::MissingAssetName,
                base + ".name",
                "asset name is required",
                "non-empty string");
        } else if (resource_name_seen(
                       catalog.assets,
                       i,
                       [](AssetDescriptor const& value)
                           -> std::string_view { return value.name; })) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::DuplicateAssetName,
                base + ".name",
                "asset name must be unique",
                "unique asset name",
                asset.name);
        }
        if (asset.source.empty()) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::MissingAssetSource,
                base + ".source",
                "asset source is required",
                "non-empty path");
        }
    }

    for (std::size_t i = 0; i < catalog.locales.size(); ++i) {
        auto const& locale = catalog.locales[i];
        auto base = std::string{"locales["} + std::to_string(i) + "]";
        if (locale.tag.empty()) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::MissingLocaleTag,
                base + ".tag",
                "locale tag is required",
                "non-empty BCP-47 tag");
        } else if (resource_name_seen(
                       catalog.locales,
                       i,
                       [](LocaleDescriptor const& value)
                           -> std::string_view { return value.tag; })) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::DuplicateLocaleTag,
                base + ".tag",
                "locale tag must be unique",
                "unique locale tag",
                locale.tag);
        }
        if (locale.source.empty()) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::MissingLocaleSource,
                base + ".source",
                "locale source is required",
                "non-empty path");
        }
    }

    for (std::size_t i = 0; i < catalog.fonts.size(); ++i) {
        auto const& font = catalog.fonts[i];
        auto base = std::string{"fonts["} + std::to_string(i) + "]";
        if (font.family.empty()) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::MissingFontFamily,
                base + ".family",
                "font family is required",
                "non-empty font family");
        } else if (resource_name_seen(
                       catalog.fonts,
                       i,
                       [](FontDescriptor const& value)
                           -> std::string_view { return value.family; })) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::DuplicateFontFamily,
                base + ".family",
                "font family must be unique",
                "unique font family",
                font.family);
        }
        if (font.source.empty()) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::MissingFontSource,
                base + ".source",
                "font source is required",
                "non-empty path");
        }
    }

    if (catalog.debug.artifact_manifest.empty()) {
        add_resource_diagnostic(
            diagnostics,
            ResourceDiagnosticKind::MissingArtifactManifest,
            "debug.artifact_manifest",
            "debug artifact manifest is required for LLM-debuggable packages",
            "non-empty path");
    }
    if (catalog.debug.verifier.empty()) {
        add_resource_diagnostic(
            diagnostics,
            ResourceDiagnosticKind::MissingDebugVerifier,
            "debug.verifier",
            "debug verifier command is required for local package gates",
            "non-empty command");
    }

    for (auto key : required_locale_keys) {
        if (key.empty())
            continue;
        if (!localized_string(catalog, key, catalog.default_locale)) {
            add_resource_diagnostic(
                diagnostics,
                ResourceDiagnosticKind::MissingLocaleKey,
                std::string{"locales."} + catalog.default_locale + "." + std::string{key},
                "default locale cannot resolve required key",
                std::string{key});
        }
        for (auto const& locale : catalog.locales) {
            if (locale.tag.empty())
                continue;
            if (!localized_string(catalog, key, locale.tag)) {
                add_resource_diagnostic(
                    diagnostics,
                    ResourceDiagnosticKind::MissingLocaleKey,
                    std::string{"locales."} + locale.tag + "." + std::string{key},
                    "locale fallback chain cannot resolve required key",
                    std::string{key});
            }
        }
    }

    return diagnostics;
}

inline bool resource_catalog_ok(
        ResourceCatalog const& catalog,
        std::span<std::string_view const> required_locale_keys = {}) {
    auto diagnostics = validate_resource_catalog(catalog, required_locale_keys);
    return std::ranges::none_of(diagnostics, [](ResourceDiagnostic const& d) {
        return d.severity == ResourceDiagnosticSeverity::Error;
    });
}

} // namespace phenotype

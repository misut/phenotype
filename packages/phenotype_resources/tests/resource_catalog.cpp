#include <array>
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
        .source = "assets/icon.txt",
        .content_type = "text/plain",
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
        .fallback = {"system-ui"},
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

    catalog.assets.push_back({
        .name = "app.icon",
        .source = "assets/duplicate.txt",
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

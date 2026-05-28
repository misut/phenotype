#include <cassert>
#include <algorithm>
#include <string>

import phenotype_cli.package;

int main() {
    namespace cli_package = phenotype_cli::package;

    auto summary = cli_package::PackageSummary{};
    summary.svg_asset_inspections.push_back(
        {.name = "app.icon",
         .source = "assets/app-icon.svg",
         .present = true,
         .inspected = true,
         .ok = true});
    summary.svg_asset_inspections.push_back(
        {.name = "file.pdf",
         .source = "assets/file-pdf.svg",
         .present = true,
         .inspected = true,
         .ok = true,
         .native_window_control_palette_hits = {"#ff5f57"}});
    summary.svg_asset_inspections.push_back(
        {.name = "file_type.pdf.icon",
         .source = "assets/icons/file-types/pdf.svg",
         .present = true,
         .sha256 = std::string(64, 'a'),
         .inspected = true,
         .ok = true,
         .file_type_icon_asset = true,
         .file_type_token = "pdf",
         .icon_symbol = "pdf_document",
         .source_attribution = {
             .family = "Google Material Symbols",
             .icon_name = "picture_as_pdf",
             .style = "outlined",
             .license = "Apache-2.0",
             .license_url = "https://example.invalid/LICENSE",
             .source_url = "https://example.invalid/picture_as_pdf.svg",
             .source_revision = "4d767880",
             .embedded_source = true,
             .platform_extracted = false,
             .runtime_fetch_required = false},
         .catalog_source_inspected = true,
         .catalog_source_ok = true,
         .catalog_source_shape_match = true,
         .catalog_source_canonical_match = true});

    auto checks = cli_package::package_checks(summary);
    auto find_check = [&](std::string const& name) {
        auto found = std::ranges::find_if(
            checks,
            [&](auto const& check) {
                return check.name == name;
            });
        assert(found != checks.end());
        return *found;
    };

    auto app_icon_check = find_check("app_icon_native_chrome_palette");
    assert(app_icon_check.ok);

    auto asset_check = find_check("svg_assets_native_chrome_palette");
    assert(!asset_check.ok);
    assert(asset_check.detail.find("file.pdf=[#ff5f57]")
        != std::string::npos);

    auto provenance_check = find_check("svg_file_type_icon_provenance");
    assert(provenance_check.ok);
    assert(provenance_check.detail.find("file_type_icon_assets=1")
        != std::string::npos);
    assert(provenance_check.detail.find("digest_failures=0")
        != std::string::npos);
    assert(provenance_check.detail.find("catalog_source_failures=0")
        != std::string::npos);
}

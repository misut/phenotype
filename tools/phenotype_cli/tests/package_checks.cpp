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
}

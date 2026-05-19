export module phenotype_cli.package_svg;

export import phenotype_cli.package_types;

import cppx.checksum;
import cppx.checksum.system;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.svg_contract;
import phenotype_cli.common;
import std;

namespace phenotype_cli::package {

namespace fs = std::filesystem;
namespace icon_catalog = phenotype::icon_catalog;
namespace svg_contract = phenotype::svg_contract;
using namespace phenotype_cli::common;

auto svg_sha256_file_or_error(fs::path const& path)
    -> std::expected<std::string, std::string> {
    auto digest = cppx::checksum::system::sha256_file(path);
    if (digest)
        return *digest;
    return std::unexpected{std::format(
        "{}: {}",
        cppx::checksum::to_string(digest.error().code),
        digest.error().message)};
}

auto file_type_token_from_package_asset_name(std::string_view name)
        -> std::optional<std::string_view> {
    auto constexpr prefix = std::string_view{"file_type."};
    auto constexpr suffix = std::string_view{".icon"};
    if (!name.starts_with(prefix) || !name.ends_with(suffix))
        return std::nullopt;
    return name.substr(prefix.size(),
                       name.size() - prefix.size() - suffix.size());
}

auto file_type_symbol_for_token(std::string_view token)
        -> std::optional<icon_catalog::Symbol> {
    if (token == "folder")
        return icon_catalog::Symbol::Folder;
    if (token == "document")
        return icon_catalog::Symbol::Document;
    if (token == "pdf")
        return icon_catalog::Symbol::PdfDocument;
    if (token == "text")
        return icon_catalog::Symbol::TextDocument;
    if (token == "image")
        return icon_catalog::Symbol::Image;
    if (token == "movie")
        return icon_catalog::Symbol::Movie;
    if (token == "archive")
        return icon_catalog::Symbol::Archive;
    if (token == "audio")
        return icon_catalog::Symbol::AudioDocument;
    if (token == "code")
        return icon_catalog::Symbol::CodeDocument;
    if (token == "spreadsheet")
        return icon_catalog::Symbol::SpreadsheetDocument;
    if (token == "presentation")
        return icon_catalog::Symbol::PresentationDocument;
    return std::nullopt;
}

auto package_icon_source_attribution(
        icon_catalog::Symbol symbol) -> PackageIconSourceAttribution {
    auto source = icon_catalog::source_attribution(symbol);
    return PackageIconSourceAttribution{
        .family = std::string{source.family},
        .icon_name = std::string{source.icon_name},
        .license = std::string{source.license},
        .license_url = std::string{source.license_url},
        .source_url = std::string{source.source_url},
        .source_revision = std::string{source.source_revision},
        .copyright = std::string{source.copyright},
        .embedded_source = source.embedded_source,
        .modified_for_phenotype = source.modified_for_phenotype,
        .apple_asset = source.apple_asset,
        .platform_extracted = source.platform_extracted,
        .runtime_fetch_required = source.runtime_fetch_required,
    };
}

void attach_file_type_icon_provenance(PackageSvgAssetInspection& inspection) {
    auto token = file_type_token_from_package_asset_name(inspection.name);
    if (!token)
        return;
    inspection.file_type_icon_asset = true;
    inspection.file_type_token = std::string{*token};
    auto symbol = file_type_symbol_for_token(*token);
    if (!symbol)
        return;
    inspection.icon_symbol = std::string{icon_catalog::name(*symbol)};
    inspection.source_attribution = package_icon_source_attribution(*symbol);
}

bool same_svg_summary(svg_contract::DocumentSummary const& lhs,
                      svg_contract::DocumentSummary const& rhs) {
    auto same_float = [](float a, float b) {
        return std::abs(a - b) < 0.001f;
    };
    return same_float(lhs.view_min_x, rhs.view_min_x)
        && same_float(lhs.view_min_y, rhs.view_min_y)
        && same_float(lhs.view_width, rhs.view_width)
        && same_float(lhs.view_height, rhs.view_height)
        && lhs.shape_count == rhs.shape_count
        && lhs.unsupported_count == rhs.unsupported_count
        && lhs.paintable == rhs.paintable;
}

bool same_canonical_svg_source(std::string_view lhs,
                               std::string_view rhs) {
    auto lhs_it = lhs.begin();
    auto rhs_it = rhs.begin();
    bool lhs_quoted = false;
    bool rhs_quoted = false;
    char lhs_quote = '\0';
    char rhs_quote = '\0';
    auto next_relevant = [](std::string_view::iterator& it,
                            std::string_view::iterator end,
                            bool quoted) {
        while (!quoted && it != end
               && std::isspace(static_cast<unsigned char>(*it))) {
            ++it;
        }
    };
    auto advance_quote = [](char ch, bool& quoted, char& quote) {
        if (quoted) {
            if (ch == quote) {
                quoted = false;
                quote = '\0';
            }
            return;
        }
        if (ch == '"' || ch == '\'') {
            quoted = true;
            quote = ch;
        }
    };
    while (true) {
        next_relevant(lhs_it, lhs.end(), lhs_quoted);
        next_relevant(rhs_it, rhs.end(), rhs_quoted);
        if (lhs_it == lhs.end() || rhs_it == rhs.end())
            return lhs_it == lhs.end() && rhs_it == rhs.end();
        if (*lhs_it != *rhs_it)
            return false;
        advance_quote(*lhs_it, lhs_quoted, lhs_quote);
        advance_quote(*rhs_it, rhs_quoted, rhs_quote);
        ++lhs_it;
        ++rhs_it;
    }
}

void attach_file_type_catalog_source_contract(
        PackageSvgAssetInspection& inspection,
        std::string_view package_source) {
    if (!inspection.file_type_icon_asset || inspection.file_type_token.empty())
        return;
    auto symbol = file_type_symbol_for_token(inspection.file_type_token);
    if (!symbol)
        return;

    auto const catalog_source = icon_catalog::svg_source(*symbol);
    inspection.catalog_source_bytes = catalog_source.size();
    inspection.catalog_source_canonical_match =
        same_canonical_svg_source(package_source, catalog_source);
    inspection.catalog_source_inspected = true;
    inspection.catalog_source_inspection =
        svg_contract::inspect_source(catalog_source);
    auto const& source_summary =
        inspection.catalog_source_inspection.summary;
    inspection.catalog_source_ok = source_summary.paintable
        && source_summary.unsupported_count == 0
        && !source_summary.has_diagnostics;
    inspection.catalog_source_shape_match = same_svg_summary(
        inspection.inspection.summary,
        source_summary);
}

auto inspect_package_svg_asset(
        fs::path const& root,
        phenotype::AssetDescriptor const& asset)
    -> PackageSvgAssetInspection {
    auto result = PackageSvgAssetInspection{};
    result.name = asset.name;
    result.source = asset.source;
    attach_file_type_icon_provenance(result);
    result.path = root / asset.source;
    result.present = path_exists(result.path);
    result.bytes = file_size_or_zero(result.path);
    if (!result.present) {
        result.error = "asset source is missing";
        return result;
    }
    if (auto digest = svg_sha256_file_or_error(result.path)) {
        result.sha256 = *digest;
    } else {
        result.integrity_error = digest.error();
    }

    auto const source = read_text_file(result.path);
    if (source.empty() && result.bytes > 0) {
        result.error = "failed to read SVG asset source";
        return result;
    }

    auto lower_source = source;
    std::ranges::transform(lower_source, lower_source.begin(),
                           [](unsigned char ch) {
                               return static_cast<char>(std::tolower(ch));
                           });
    for (auto marker_color : {
             std::string_view{"#ff5f57"},
             std::string_view{"#ff5f56"},
             std::string_view{"#ffbd2e"},
             std::string_view{"#28c840"},
             std::string_view{"#27c93f"},
         }) {
        if (lower_source.find(marker_color) != std::string::npos)
            result.native_window_control_palette_hits.push_back(
                std::string{marker_color});
    }

    result.inspected = true;
    result.inspection = svg_contract::inspect_source(source);
    auto const& summary = result.inspection.summary;
    result.ok = summary.paintable
        && summary.unsupported_count == 0
        && !summary.has_diagnostics;
    attach_file_type_catalog_source_contract(result, source);
    return result;
}

export auto inspect_package_svg_assets(
        fs::path const& root,
        phenotype::ResourceCatalog const& catalog)
    -> std::vector<PackageSvgAssetInspection> {
    auto inspections = std::vector<PackageSvgAssetInspection>{};
    for (auto const& asset : catalog.assets) {
        if (!phenotype::resource_asset_declares_svg(asset))
            continue;
        inspections.push_back(inspect_package_svg_asset(root, asset));
    }
    return inspections;
}

export auto svg_asset_inspection_failure_count(
        PackageSummary const& summary) -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(
        summary.svg_asset_inspections,
        [](auto const& inspection) {
            return !inspection.ok;
        }));
}

export bool svg_asset_inspections_ok(PackageSummary const& summary) {
    return summary.resource_contract.svg_asset_count
            == summary.svg_asset_inspections.size()
        && summary.resource_contract.svg_asset_count > 0
        && svg_asset_inspection_failure_count(summary) == 0;
}

export auto app_icon_native_window_palette_failure_count(
        PackageSummary const& summary) -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(
        summary.svg_asset_inspections,
        [](auto const& inspection) {
            return inspection.name == "app.icon"
                && !inspection.native_window_control_palette_hits.empty();
        }));
}

export auto native_window_palette_failure_count(
        PackageSummary const& summary) -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(
        summary.svg_asset_inspections,
        [](auto const& inspection) {
            return !inspection.native_window_control_palette_hits.empty();
        }));
}

export auto native_window_palette_failure_detail(
        PackageSummary const& summary) -> std::string {
    auto const failure_count =
        native_window_palette_failure_count(summary);
    if (failure_count == 0)
        return "asset_marker_palette_failures=0";

    auto detail = std::format("asset_marker_palette_failures={}",
                              failure_count);
    auto shown = std::size_t{0};
    for (auto const& inspection : summary.svg_asset_inspections) {
        if (inspection.native_window_control_palette_hits.empty())
            continue;
        if (shown >= 4) {
            detail += std::format(" (+{} more)", failure_count - shown);
            break;
        }
        detail += std::format(" {}=[",
                              inspection.name.empty()
                                  ? path_string(inspection.path.filename())
                                  : inspection.name);
        for (std::size_t i = 0;
             i < inspection.native_window_control_palette_hits.size();
             ++i) {
            if (i > 0)
                detail += ",";
            detail += inspection.native_window_control_palette_hits[i];
        }
        detail += "]";
        ++shown;
    }
    return detail;
}

auto file_type_icon_asset_count(PackageSummary const& summary) -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(
        summary.svg_asset_inspections,
        [](auto const& inspection) {
            return inspection.file_type_icon_asset;
        }));
}

auto file_type_icon_provenance_failure_count(
        PackageSummary const& summary) -> std::size_t {
    return static_cast<std::size_t>(std::ranges::count_if(
        summary.svg_asset_inspections,
        [](auto const& inspection) {
            if (!inspection.file_type_icon_asset)
                return false;
            auto const& source = inspection.source_attribution;
            return inspection.file_type_token.empty()
                || inspection.icon_symbol.empty()
                || source.family.empty()
                || source.license.empty()
                || source.source_url.empty()
                || source.source_revision.empty()
                || !source.embedded_source
                || source.apple_asset
                || source.platform_extracted
                || source.runtime_fetch_required
                || inspection.sha256.empty()
                || !inspection.integrity_error.empty()
                || !inspection.catalog_source_inspected
                || !inspection.catalog_source_ok
                || !inspection.catalog_source_shape_match
                || !inspection.catalog_source_canonical_match;
        }));
}

export auto file_type_icon_provenance_detail(
        PackageSummary const& summary) -> std::string {
    auto const icon_count = file_type_icon_asset_count(summary);
    auto const failure_count =
        file_type_icon_provenance_failure_count(summary);
    auto digest_failure_count = std::size_t{0};
    auto catalog_source_failure_count = std::size_t{0};
    for (auto const& inspection : summary.svg_asset_inspections) {
        if (!inspection.file_type_icon_asset)
            continue;
        if (inspection.sha256.empty() || !inspection.integrity_error.empty())
            ++digest_failure_count;
        if (!inspection.catalog_source_inspected
            || !inspection.catalog_source_ok
            || !inspection.catalog_source_shape_match
            || !inspection.catalog_source_canonical_match) {
            ++catalog_source_failure_count;
        }
    }
    auto detail = std::format(
        "file_type_icon_assets={} failures={} "
        "digest_failures={} catalog_source_failures={}",
        icon_count,
        failure_count,
        digest_failure_count,
        catalog_source_failure_count);
    if (failure_count == 0)
        return detail;

    auto shown = std::size_t{0};
    for (auto const& inspection : summary.svg_asset_inspections) {
        if (!inspection.file_type_icon_asset)
            continue;
        auto const& source = inspection.source_attribution;
        auto failed = inspection.file_type_token.empty()
            || inspection.icon_symbol.empty()
            || source.family.empty()
            || source.license.empty()
            || source.source_url.empty()
            || source.source_revision.empty()
            || !source.embedded_source
            || source.apple_asset
            || source.platform_extracted
            || source.runtime_fetch_required
            || inspection.sha256.empty()
            || !inspection.integrity_error.empty()
            || !inspection.catalog_source_inspected
            || !inspection.catalog_source_ok
            || !inspection.catalog_source_shape_match
            || !inspection.catalog_source_canonical_match;
        if (!failed)
            continue;
        if (shown >= 4) {
            detail += std::format(" (+{} more)", failure_count - shown);
            break;
        }
        detail += std::format(
            " {} token={} family={} license={} fetch={} platform={} "
            "digest={} catalog_source_ok={} shape_match={} canonical_match={}",
            inspection.name,
            inspection.file_type_token.empty() ? "<missing>" : inspection.file_type_token,
            source.family.empty() ? "<missing>" : source.family,
            source.license.empty() ? "<missing>" : source.license,
            source.runtime_fetch_required ? "true" : "false",
            source.platform_extracted ? "true" : "false",
            inspection.sha256.empty() ? "<missing>" : "present",
            inspection.catalog_source_ok ? "true" : "false",
            inspection.catalog_source_shape_match ? "true" : "false",
            inspection.catalog_source_canonical_match ? "true" : "false");
        ++shown;
    }
    return detail;
}

export bool file_type_icon_provenance_ok(PackageSummary const& summary) {
    return file_type_icon_provenance_failure_count(summary) == 0;
}

export auto svg_document_summary_json(
        svg_contract::DocumentSummary const& summary) -> std::string {
    return std::format(
        "{{\"view_box\":{{\"x\":{},\"y\":{},\"width\":{},\"height\":{}}},"
        "\"shape_count\":{},\"diagnostic_count\":{},"
        "\"unsupported_count\":{},\"paintable\":{},"
        "\"has_diagnostics\":{}}}",
        summary.view_min_x,
        summary.view_min_y,
        summary.view_width,
        summary.view_height,
        summary.shape_count,
        summary.diagnostic_count,
        summary.unsupported_count,
        summary.paintable ? "true" : "false",
        summary.has_diagnostics ? "true" : "false");
}

export auto icon_source_attribution_json(
        PackageIconSourceAttribution const& source) -> std::string {
    return std::format(
        "{{\"family\":{},\"icon_name\":{},\"license\":{},"
        "\"license_url\":{},\"source_url\":{},\"source_revision\":{},"
        "\"copyright\":{},\"embedded_source\":{},"
        "\"modified_for_phenotype\":{},\"apple_asset\":{},"
        "\"platform_extracted\":{},\"runtime_fetch_required\":{}}}",
        json_string(source.family),
        json_string(source.icon_name),
        json_string(source.license),
        json_string(source.license_url),
        json_string(source.source_url),
        json_string(source.source_revision),
        json_string(source.copyright),
        source.embedded_source ? "true" : "false",
        source.modified_for_phenotype ? "true" : "false",
        source.apple_asset ? "true" : "false",
        source.platform_extracted ? "true" : "false",
        source.runtime_fetch_required ? "true" : "false");
}

export auto svg_asset_inspection_json(
        PackageSvgAssetInspection const& inspection) -> std::string {
    return std::format(
        "{{\"name\":{},\"source\":{},\"path\":{},\"present\":{},"
        "\"bytes\":{},\"sha256\":{},\"integrity_error\":{},"
        "\"inspected\":{},\"ok\":{},\"error\":{},"
        "\"native_window_control_palette_hits\":{},"
        "\"file_type_icon_asset\":{},\"file_type_token\":{},"
        "\"icon_symbol\":{},\"source_attribution\":{},"
        "\"catalog_source\":{{\"inspected\":{},\"ok\":{},"
        "\"shape_match\":{},\"canonical_match\":{},\"bytes\":{},\"summary\":{},"
        "\"diagnostics\":{}}},"
        "\"summary\":{},\"diagnostics\":{}}}",
        json_string(inspection.name),
        json_string(inspection.source),
        json_string(path_string(inspection.path)),
        inspection.present ? "true" : "false",
        inspection.bytes,
        json_string(inspection.sha256),
        json_string(inspection.integrity_error),
        inspection.inspected ? "true" : "false",
        inspection.ok ? "true" : "false",
        json_string(inspection.error),
        string_array_json(inspection.native_window_control_palette_hits),
        inspection.file_type_icon_asset ? "true" : "false",
        json_string(inspection.file_type_token),
        json_string(inspection.icon_symbol),
        icon_source_attribution_json(inspection.source_attribution),
        inspection.catalog_source_inspected ? "true" : "false",
        inspection.catalog_source_ok ? "true" : "false",
        inspection.catalog_source_shape_match ? "true" : "false",
        inspection.catalog_source_canonical_match ? "true" : "false",
        inspection.catalog_source_bytes,
        svg_document_summary_json(
            inspection.catalog_source_inspection.summary),
        string_array_json(inspection.catalog_source_inspection.diagnostics),
        svg_document_summary_json(inspection.inspection.summary),
        string_array_json(inspection.inspection.diagnostics));
}

export auto svg_asset_inspections_json(
        std::span<PackageSvgAssetInspection const> inspections)
    -> std::string {
    auto out = std::string{"["};
    for (std::size_t i = 0; i < inspections.size(); ++i) {
        if (i > 0)
            out += ",";
        out += svg_asset_inspection_json(inspections[i]);
    }
    out += "]";
    return out;
}

}

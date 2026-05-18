export module phenotype_cli.package_types;

import phenotype.resources;
import phenotype.svg_contract;
import phenotype_cli.common;
import std;

namespace phenotype_cli::package {

namespace fs = std::filesystem;
namespace svg_contract = phenotype::svg_contract;
using phenotype_cli::common::Check;

export struct PackageIconSourceAttribution {
    std::string family;
    std::string icon_name;
    std::string license;
    std::string license_url;
    std::string source_url;
    std::string source_revision;
    std::string copyright;
    bool embedded_source = false;
    bool modified_for_phenotype = false;
    bool apple_asset = false;
    bool platform_extracted = false;
    bool runtime_fetch_required = false;
};

export struct PackageSvgAssetInspection {
    std::string name;
    std::string source;
    fs::path path;
    bool present = false;
    std::uintmax_t bytes = 0;
    std::string sha256;
    std::string integrity_error;
    bool inspected = false;
    bool ok = false;
    std::string error;
    std::vector<std::string> native_window_control_palette_hits;
    bool file_type_icon_asset = false;
    std::string file_type_token;
    std::string icon_symbol;
    PackageIconSourceAttribution source_attribution;
    svg_contract::DocumentInspection inspection;
    bool catalog_source_inspected = false;
    bool catalog_source_ok = false;
    bool catalog_source_shape_match = false;
    std::uintmax_t catalog_source_bytes = 0;
    svg_contract::DocumentInspection catalog_source_inspection;
};

export struct PackageSummary {
    fs::path root;
    bool exists = false;
    bool is_directory = false;
    bool manifest = false;
    bool application_section = false;
    bool resources_section = false;
    bool debug_section = false;
    bool artifact_manifest_exists = false;
    bool default_font_pretendard = false;
    bool assets_directory = false;
    bool locales_directory = false;
    bool fonts_directory = false;
    std::string application_id;
    std::string display_name;
    std::string version;
    std::string entry;
    std::vector<std::string> platforms;
    std::string default_font_family;
    std::string artifact_manifest;
    std::string debug_probe_scene;
    std::string debug_verifier;
    bool app_icon_declared = false;
    bool app_icon_svg = false;
    bool app_icon_preload = false;
    bool default_font_has_cjk_fallback = false;
    phenotype::ResourceCatalog catalog;
    std::vector<phenotype::ResourceDiagnostic> catalog_diagnostics;
    phenotype::ResourceCatalogContract resource_contract;
    std::vector<PackageSvgAssetInspection> svg_asset_inspections;
    std::vector<std::string> missing_sources;
    std::uintmax_t manifest_bytes = 0;
    std::size_t manifest_asset_count = 0;
    std::size_t manifest_locale_count = 0;
    std::size_t manifest_font_count = 0;
    std::size_t source_reference_count = 0;
    std::size_t required_locale_key_count = 0;
    std::size_t locale_string_count = 0;
    std::size_t asset_file_count = 0;
    std::size_t locale_file_count = 0;
    std::size_t font_file_count = 0;
};

export struct BundleFileRecord {
    std::string kind;
    std::string name;
    std::string content_type;
    fs::path source;
    fs::path destination;
    std::uintmax_t bytes = 0;
    bool present = false;
    bool copied = false;
    bool preload = false;
    bool runtime_visible = false;
    std::string sha256;
    std::string integrity_error;
    std::string error;
};

export struct BundleManifestRecord {
    std::string destination;
    std::string digest;
};

export struct StoredBundleManifest {
    bool present = false;
    bool parse_ok = false;
    std::string parse_error;
    std::int64_t schema_version = -1;
    std::string command;
    std::int64_t file_count = -1;
    std::int64_t total_bytes = -1;
    bool integrity_ok = false;
    std::vector<BundleManifestRecord> records;
    std::size_t matched_digest_count = 0;
    std::size_t missing_record_count = 0;
    std::size_t digest_mismatch_count = 0;
};

export struct BundleSummary {
    std::string command = "package bundle";
    std::string format = "resources";
    fs::path package_root;
    fs::path output_root;
    fs::path resource_root;
    fs::path manifest_path;
    PackageSummary package;
    std::vector<BundleFileRecord> files;
    std::vector<Check> checks;
    std::uintmax_t total_bytes = 0;
    std::size_t verified_file_count = 0;
    bool bundle_manifest_present = false;
    std::uintmax_t bundle_manifest_bytes = 0;
    StoredBundleManifest stored_manifest;
    bool ok = false;
    std::string error;
};

} // namespace phenotype_cli::package

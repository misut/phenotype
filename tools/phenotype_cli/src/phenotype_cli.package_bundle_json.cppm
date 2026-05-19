export module phenotype_cli.package_bundle_json;

import json;
import phenotype_cli.common;
import phenotype_cli.package_json;
export import phenotype_cli.package_types;
import std;

export namespace phenotype_cli::package {

namespace fs = std::filesystem;
using namespace phenotype_cli::common;

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
            ? "true"
            : "false",
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

} // namespace phenotype_cli::package

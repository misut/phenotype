export module phenotype_cli.package;

export import phenotype_cli.package_types;
export import phenotype_cli.package_bundle_json;
export import phenotype_cli.package_inspect;
export import phenotype_cli.package_json;

import cppx.checksum;
import cppx.checksum.system;
import cppx.cli;
import cppx.process;
import cppx.process.system;
import cppx.terminal;
import phenotype.resources;
import phenotype_cli.common;
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

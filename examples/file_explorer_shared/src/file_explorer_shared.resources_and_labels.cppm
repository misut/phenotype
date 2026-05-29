module;
#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module file_explorer_shared:resources_and_labels;

import json;
import phenotype.icon_catalog;
import phenotype.resources;
import phenotype.theme_contract;
import :model_types;
import :desktop_metrics_and_symbols;
import :viewport_and_focus_helpers;
import :chrome_and_geometry;
import :filesystem_model;
import :icon_and_interaction_debug_json;
import :chrome_debug_json;
import :resources_debug_json;
import :debug_payload_json;
import :input_parsing;
import :filesystem_snapshot_helpers;
import :state_factories;
import :navigation_and_file_actions;
import :focus_and_scripted_inputs;
import :drive_and_expectations;

export namespace file_explorer_demo {
struct ExplorerLabels {
    std::string title = "Recents";
    std::string mobile_title = "Files";
    std::string sidebar_recents = "Recents";
    std::string sidebar_shared = "Shared";
    std::string favorites = "Favorites";
    std::string locations = "Locations";
    std::string applications = "Applications";
    std::string desktop = "Desktop";
    std::string documents = "Documents";
    std::string pictures = "Pictures";
    std::string downloads = "Downloads";
    std::string icloud_drive = "iCloud Drive";
    std::string home = "kakao";
    std::string airdrop = "AirDrop";
    std::string trash = "Trash";
    std::string search_placeholder = "Search";
    std::string status_ready = "Ready";
    std::string tab_browse = "Browse";
    std::string tab_preview = "Preview";
    std::string tab_create = "Create";
    std::string create_file = "New File";
    std::string create_folder = "New Folder";
    std::string duplicate = "Duplicate";
    std::string duplicate_file = "Duplicate File";
    std::string delete_action = "Delete";
    std::string delete_selected = "Delete Selected";
    std::string preview = "Preview";
    std::string root = "Root";
    std::string docs = "Docs";
    std::string pics = "Pics";
    std::string back = "Back";
    std::string forward = "Forward";
    std::string up = "Up";
    std::string sort = "Sort";
    std::string name = "Name";
    std::string kind = "Kind";
    std::string size = "Size";
    std::string no_matching_files = "No matching files.";
    std::string select_file_to_preview = "Select a file to preview.";
    std::string select_file_from_browse = "Select a file from Browse.";
    std::string create_hint =
        "Files and folders are written only inside the demo root.";
    std::string file_name = "File name";
    std::string contents = "Contents";
    std::string folder_name = "Folder name";
    std::string reset_demo_files = "Reset Demo Files";
    std::string preferences = "Display";
    std::string preferences_system_font = "System";
    std::string preferences_package_font = "Pretendard";
    std::string preferences_system_text_size = "OS Size";
    std::string preferences_package_text_size = "App Size";
    std::string preferences_text_larger = "Text +";
    std::string preferences_text_smaller = "Text -";
    std::string preferences_body_larger = "Body +";
    std::string preferences_body_smaller = "Body -";
    std::string preferences_heading_larger = "Heading +";
    std::string preferences_heading_smaller = "Heading -";
    std::string preferences_small_larger = "Small +";
    std::string preferences_small_smaller = "Small -";
    std::string preferences_line_height_taller = "Line +";
    std::string preferences_line_height_tighter = "Line -";
    std::string preferences_scroll_faster = "Scroll +";
    std::string preferences_scroll_slower = "Scroll -";
    std::string preferences_horizontal_scroll_faster = "H Scroll +";
    std::string preferences_horizontal_scroll_slower = "H Scroll -";
    std::string preferences_system_scroll = "System Scroll";
    std::string preferences_app_scroll = "App Scroll";
    std::string preferences_scrollbar_auto = "Bars Auto";
    std::string preferences_scrollbar_always = "Bars Always";
    std::string preferences_scrollbar_hidden = "Bars Hidden";
    std::string preferences_system_appearance = "System Look";
    std::string preferences_light_appearance = "Light";
    std::string preferences_dark_appearance = "Dark";
    std::string status = "Status";
};

inline void add_locale_strings(
        phenotype::LocaleDescriptor& locale,
        std::initializer_list<std::pair<std::string_view, std::string_view>> values) {
    for (auto const& [key, value] : values) {
        locale.strings.push_back({
            .key = std::string{key},
            .value = std::string{value},
        });
    }
}

phenotype::ResourceCatalog file_explorer_resource_catalog(
        std::string_view profile) {
    phenotype::ResourceCatalog catalog;
    bool const mobile = profile == "mobile";
    catalog.application.id = mobile
        ? "com.misut.phenotype.examples.file-explorer-mobile"
        : "com.misut.phenotype.examples.file-explorer-desktop";
    catalog.application.display_name = mobile
        ? "Phenotype Mobile File Explorer"
        : "Phenotype File Explorer";
    catalog.application.version = "0.1.0";
    catalog.application.entry = mobile
        ? "file_explorer_mobile"
        : "file_explorer_desktop";
    catalog.application.platforms = {"macos", "windows"};
    catalog.default_locale = "en";
    catalog.default_font_family = "Pretendard";
    catalog.assets.push_back({
        .name = "app.icon",
        .source = "assets/file-explorer-icon.svg",
        .content_type = "image/svg+xml",
        .preload = true,
    });
    for (auto const& asset : k_file_type_icon_assets) {
        catalog.assets.push_back({
            .name = std::string{asset.name},
            .source = std::string{asset.source},
            .content_type = "image/svg+xml",
            .preload = true,
            .runtime_visible = true,
        });
    }
    catalog.assets.push_back({
        .name = "license.material_symbols.icons",
        .source = std::string{file_type_icon_license_asset_source()},
        .content_type = "text/plain",
        .preload = false,
        .runtime_visible = false,
    });

    phenotype::LocaleDescriptor en{
        .tag = "en",
        .source = "locales/en.toml",
    };
    add_locale_strings(en, {
        {"app.title", "Recents"},
        {"app.mobile_title", "Files"},
        {"app.sidebar_recents", "Recents"},
        {"app.sidebar_shared", "Shared"},
        {"app.favorites", "Favorites"},
        {"app.locations", "Locations"},
        {"app.applications", "Applications"},
        {"app.desktop", "Desktop"},
        {"app.documents", "Documents"},
        {"app.pictures", "Pictures"},
        {"app.downloads", "Downloads"},
        {"app.icloud_drive", "iCloud Drive"},
        {"app.home", "kakao"},
        {"app.airdrop", "AirDrop"},
        {"app.trash", "Trash"},
        {"app.search_placeholder", "Search"},
        {"app.status_ready", "Ready"},
        {"app.tab_browse", "Browse"},
        {"app.tab_preview", "Preview"},
        {"app.tab_create", "Create"},
        {"actions.create_file", "New File"},
        {"actions.create_folder", "New Folder"},
        {"actions.duplicate", "Duplicate"},
        {"actions.duplicate_file", "Duplicate File"},
        {"actions.delete", "Delete"},
        {"actions.delete_selected", "Delete Selected"},
        {"actions.preview", "Preview"},
        {"nav.root", "Root"},
        {"nav.docs", "Docs"},
        {"nav.pics", "Pics"},
        {"nav.back", "Back"},
        {"nav.forward", "Forward"},
        {"nav.up", "Up"},
        {"nav.sort", "Sort"},
        {"table.name", "Name"},
        {"table.kind", "Kind"},
        {"table.size", "Size"},
        {"state.no_matching_files", "No matching files."},
        {"state.select_file_to_preview", "Select a file to preview."},
        {"state.select_file_from_browse", "Select a file from Browse."},
        {"create.hint", "Files and folders are written only inside the demo root."},
        {"create.file_name", "File name"},
        {"create.contents", "Contents"},
        {"create.folder_name", "Folder name"},
        {"create.reset_demo_files", "Reset Demo Files"},
        {"preferences.title", "Display"},
        {"preferences.system_font", "System"},
        {"preferences.package_font", "Pretendard"},
        {"preferences.system_text_size", "OS Size"},
        {"preferences.package_text_size", "App Size"},
        {"preferences.text_larger", "Text +"},
        {"preferences.text_smaller", "Text -"},
        {"preferences.body_larger", "Body +"},
        {"preferences.body_smaller", "Body -"},
        {"preferences.heading_larger", "Heading +"},
        {"preferences.heading_smaller", "Heading -"},
        {"preferences.small_larger", "Small +"},
        {"preferences.small_smaller", "Small -"},
        {"preferences.line_height_taller", "Line +"},
        {"preferences.line_height_tighter", "Line -"},
        {"preferences.scroll_faster", "Scroll +"},
        {"preferences.scroll_slower", "Scroll -"},
        {"preferences.horizontal_scroll_faster", "H Scroll +"},
        {"preferences.horizontal_scroll_slower", "H Scroll -"},
        {"preferences.system_scroll", "System Scroll"},
        {"preferences.app_scroll", "App Scroll"},
        {"preferences.scrollbar_auto", "Bars Auto"},
        {"preferences.scrollbar_always", "Bars Always"},
        {"preferences.scrollbar_hidden", "Bars Hidden"},
        {"preferences.system_appearance", "System Look"},
        {"preferences.light_appearance", "Light"},
        {"preferences.dark_appearance", "Dark"},
        {"status.title", "Status"},
    });
    phenotype::LocaleDescriptor ko{
        .tag = "ko",
        .source = "locales/ko.toml",
        .fallback = {"en"},
    };
    add_locale_strings(ko, {
        {"app.title", "최근 항목"},
        {"app.mobile_title", "파일"},
        {"app.sidebar_recents", "최근 항목"},
        {"app.sidebar_shared", "공유"},
        {"app.favorites", "즐겨찾기"},
        {"app.locations", "위치"},
        {"app.applications", "응용 프로그램"},
        {"app.desktop", "데스크탑"},
        {"app.documents", "문서"},
        {"app.pictures", "사진"},
        {"app.downloads", "다운로드"},
        {"app.icloud_drive", "iCloud Drive"},
        {"app.home", "kakao"},
        {"app.airdrop", "AirDrop"},
        {"app.trash", "휴지통"},
        {"app.search_placeholder", "검색"},
        {"app.status_ready", "준비됨"},
        {"app.tab_browse", "둘러보기"},
        {"app.tab_preview", "미리보기"},
        {"app.tab_create", "만들기"},
        {"actions.create_file", "새 파일"},
        {"actions.create_folder", "새 폴더"},
        {"actions.duplicate", "복제"},
        {"actions.duplicate_file", "파일 복제"},
        {"actions.delete", "삭제"},
        {"actions.delete_selected", "선택 항목 삭제"},
        {"actions.preview", "미리보기"},
        {"nav.root", "루트"},
        {"nav.docs", "문서"},
        {"nav.pics", "사진"},
        {"nav.back", "뒤로"},
        {"nav.forward", "앞으로"},
        {"nav.up", "위로"},
        {"nav.sort", "정렬"},
        {"table.name", "이름"},
        {"table.kind", "종류"},
        {"table.size", "크기"},
        {"state.no_matching_files", "일치하는 파일이 없습니다."},
        {"state.select_file_to_preview", "미리 볼 파일을 선택하세요."},
        {"state.select_file_from_browse", "둘러보기에서 파일을 선택하세요."},
        {"create.hint", "파일과 폴더는 데모 루트 안에만 기록됩니다."},
        {"create.file_name", "파일 이름"},
        {"create.contents", "내용"},
        {"create.folder_name", "폴더 이름"},
        {"create.reset_demo_files", "데모 파일 초기화"},
        {"preferences.title", "표시"},
        {"preferences.system_font", "시스템"},
        {"preferences.package_font", "Pretendard"},
        {"preferences.system_text_size", "OS 크기"},
        {"preferences.package_text_size", "앱 크기"},
        {"preferences.text_larger", "글자 +"},
        {"preferences.text_smaller", "글자 -"},
        {"preferences.body_larger", "본문 +"},
        {"preferences.body_smaller", "본문 -"},
        {"preferences.heading_larger", "제목 +"},
        {"preferences.heading_smaller", "제목 -"},
        {"preferences.small_larger", "작은 글자 +"},
        {"preferences.small_smaller", "작은 글자 -"},
        {"preferences.line_height_taller", "행간 +"},
        {"preferences.line_height_tighter", "행간 -"},
        {"preferences.scroll_faster", "스크롤 +"},
        {"preferences.scroll_slower", "스크롤 -"},
        {"preferences.horizontal_scroll_faster", "가로 스크롤 +"},
        {"preferences.horizontal_scroll_slower", "가로 스크롤 -"},
        {"preferences.system_scroll", "시스템 스크롤"},
        {"preferences.app_scroll", "앱 스크롤"},
        {"preferences.scrollbar_auto", "스크롤바 자동"},
        {"preferences.scrollbar_always", "스크롤바 항상"},
        {"preferences.scrollbar_hidden", "스크롤바 숨김"},
        {"preferences.system_appearance", "시스템"},
        {"preferences.light_appearance", "라이트"},
        {"preferences.dark_appearance", "다크"},
        {"status.title", "상태"},
    });
    catalog.locales = {std::move(en), std::move(ko)};
    catalog.fonts.push_back({
        .family = "Pretendard",
        .source = "fonts/pretendard.alias.toml",
        .fallback = {"system-ui", "Apple SD Gothic Neo", "Segoe UI", "Noto Sans CJK"},
    });
    catalog.debug.artifact_manifest = "artifact_manifest.json";
    catalog.debug.probe_scene = mobile
        ? "mobile-file-workflow"
        : "finder-style-startup";
    catalog.debug.verifier = "phenotype artifact verify-file-explorer";
    return catalog;
}

inline phenotype::ResourceCatalog file_explorer_resource_catalog_from_package_texts(
        std::string_view profile,
        std::string_view manifest_text,
        std::span<PackageResourceText const> locale_texts) {
    auto fallback = file_explorer_resource_catalog(profile);
    if (trim(manifest_text).empty())
        return fallback;

    auto parsed = phenotype::parse_resource_manifest(manifest_text);
    if (!parsed.application_section || !parsed.resources_section)
        return fallback;

    auto catalog = std::move(parsed.catalog);
    if (catalog.default_locale.empty())
        catalog.default_locale = fallback.default_locale;
    if (catalog.default_font_family.empty())
        catalog.default_font_family = fallback.default_font_family;
    for (auto& locale : catalog.locales) {
        auto found = std::ranges::find_if(
            locale_texts,
            [&](PackageResourceText const& text) {
                return text.source == locale.source;
            });
        if (found != locale_texts.end()) {
            locale.strings =
                phenotype::parse_resource_locale_strings(found->text);
        }
    }
    return catalog;
}

inline std::string localized_or(
        phenotype::ResourceCatalog const& catalog,
        std::string_view locale,
        std::string_view key,
        std::string_view fallback) {
    if (auto value = phenotype::localized_string(catalog, key, locale))
        return value->value;
    return std::string{fallback};
}

inline auto normalize_locale_tag(std::string_view tag) -> std::string {
    auto out = std::string{tag};
    for (auto& ch : out) {
        if (ch == '_')
            ch = '-';
    }
    return out;
}

inline auto resolve_supported_locale(
        phenotype::ResourceCatalog const& catalog,
        std::string_view requested) -> std::string {
    auto normalized = normalize_locale_tag(requested);
    if (normalized.empty())
        return catalog.default_locale;
    if (phenotype::find_locale(catalog, normalized))
        return normalized;
    auto separator = normalized.find('-');
    if (separator != std::string::npos) {
        auto language = normalized.substr(0, separator);
        if (phenotype::find_locale(catalog, language))
            return language;
    }
    return catalog.default_locale;
}

inline ExplorerLabels file_explorer_labels(
        std::string_view locale,
        phenotype::ResourceCatalog const& catalog) {
    ExplorerLabels labels;
    auto get = [&](std::string_view key, std::string_view fallback) {
        return localized_or(catalog, locale, key, fallback);
    };
    labels.title = get("app.title", labels.title);
    labels.mobile_title = get("app.mobile_title", labels.mobile_title);
    labels.sidebar_recents = get("app.sidebar_recents", labels.sidebar_recents);
    labels.sidebar_shared = get("app.sidebar_shared", labels.sidebar_shared);
    labels.favorites = get("app.favorites", labels.favorites);
    labels.locations = get("app.locations", labels.locations);
    labels.applications = get("app.applications", labels.applications);
    labels.desktop = get("app.desktop", labels.desktop);
    labels.documents = get("app.documents", labels.documents);
    labels.pictures = get("app.pictures", labels.pictures);
    labels.downloads = get("app.downloads", labels.downloads);
    labels.icloud_drive = get("app.icloud_drive", labels.icloud_drive);
    labels.home = get("app.home", labels.home);
    labels.airdrop = get("app.airdrop", labels.airdrop);
    labels.trash = get("app.trash", labels.trash);
    labels.search_placeholder = get("app.search_placeholder", labels.search_placeholder);
    labels.status_ready = get("app.status_ready", labels.status_ready);
    labels.tab_browse = get("app.tab_browse", labels.tab_browse);
    labels.tab_preview = get("app.tab_preview", labels.tab_preview);
    labels.tab_create = get("app.tab_create", labels.tab_create);
    labels.create_file = get("actions.create_file", labels.create_file);
    labels.create_folder = get("actions.create_folder", labels.create_folder);
    labels.duplicate = get("actions.duplicate", labels.duplicate);
    labels.duplicate_file = get("actions.duplicate_file", labels.duplicate_file);
    labels.delete_action = get("actions.delete", labels.delete_action);
    labels.delete_selected = get("actions.delete_selected", labels.delete_selected);
    labels.preview = get("actions.preview", labels.preview);
    labels.root = get("nav.root", labels.root);
    labels.docs = get("nav.docs", labels.docs);
    labels.pics = get("nav.pics", labels.pics);
    labels.back = get("nav.back", labels.back);
    labels.forward = get("nav.forward", labels.forward);
    labels.up = get("nav.up", labels.up);
    labels.sort = get("nav.sort", labels.sort);
    labels.name = get("table.name", labels.name);
    labels.kind = get("table.kind", labels.kind);
    labels.size = get("table.size", labels.size);
    labels.no_matching_files = get("state.no_matching_files", labels.no_matching_files);
    labels.select_file_to_preview = get(
        "state.select_file_to_preview",
        labels.select_file_to_preview);
    labels.select_file_from_browse = get(
        "state.select_file_from_browse",
        labels.select_file_from_browse);
    labels.create_hint = get("create.hint", labels.create_hint);
    labels.file_name = get("create.file_name", labels.file_name);
    labels.contents = get("create.contents", labels.contents);
    labels.folder_name = get("create.folder_name", labels.folder_name);
    labels.reset_demo_files = get("create.reset_demo_files", labels.reset_demo_files);
    labels.preferences = get("preferences.title", labels.preferences);
    labels.preferences_system_font = get(
        "preferences.system_font",
        labels.preferences_system_font);
    labels.preferences_package_font = get(
        "preferences.package_font",
        labels.preferences_package_font);
    labels.preferences_system_text_size = get(
        "preferences.system_text_size",
        labels.preferences_system_text_size);
    labels.preferences_package_text_size = get(
        "preferences.package_text_size",
        labels.preferences_package_text_size);
    labels.preferences_text_larger = get(
        "preferences.text_larger",
        labels.preferences_text_larger);
    labels.preferences_text_smaller = get(
        "preferences.text_smaller",
        labels.preferences_text_smaller);
    labels.preferences_body_larger = get(
        "preferences.body_larger",
        labels.preferences_body_larger);
    labels.preferences_body_smaller = get(
        "preferences.body_smaller",
        labels.preferences_body_smaller);
    labels.preferences_heading_larger = get(
        "preferences.heading_larger",
        labels.preferences_heading_larger);
    labels.preferences_heading_smaller = get(
        "preferences.heading_smaller",
        labels.preferences_heading_smaller);
    labels.preferences_small_larger = get(
        "preferences.small_larger",
        labels.preferences_small_larger);
    labels.preferences_small_smaller = get(
        "preferences.small_smaller",
        labels.preferences_small_smaller);
    labels.preferences_line_height_taller = get(
        "preferences.line_height_taller",
        labels.preferences_line_height_taller);
    labels.preferences_line_height_tighter = get(
        "preferences.line_height_tighter",
        labels.preferences_line_height_tighter);
    labels.preferences_scroll_faster = get(
        "preferences.scroll_faster",
        labels.preferences_scroll_faster);
    labels.preferences_scroll_slower = get(
        "preferences.scroll_slower",
        labels.preferences_scroll_slower);
    labels.preferences_horizontal_scroll_faster = get(
        "preferences.horizontal_scroll_faster",
        labels.preferences_horizontal_scroll_faster);
    labels.preferences_horizontal_scroll_slower = get(
        "preferences.horizontal_scroll_slower",
        labels.preferences_horizontal_scroll_slower);
    labels.preferences_system_scroll = get(
        "preferences.system_scroll",
        labels.preferences_system_scroll);
    labels.preferences_app_scroll = get(
        "preferences.app_scroll",
        labels.preferences_app_scroll);
    labels.preferences_scrollbar_auto = get(
        "preferences.scrollbar_auto",
        labels.preferences_scrollbar_auto);
    labels.preferences_scrollbar_always = get(
        "preferences.scrollbar_always",
        labels.preferences_scrollbar_always);
    labels.preferences_scrollbar_hidden = get(
        "preferences.scrollbar_hidden",
        labels.preferences_scrollbar_hidden);
    labels.preferences_system_appearance = get(
        "preferences.system_appearance",
        labels.preferences_system_appearance);
    labels.preferences_light_appearance = get(
        "preferences.light_appearance",
        labels.preferences_light_appearance);
    labels.preferences_dark_appearance = get(
        "preferences.dark_appearance",
        labels.preferences_dark_appearance);
    labels.status = get("status.title", labels.status);
    return labels;
}

inline ExplorerLabels file_explorer_labels(
        std::string_view locale,
        std::string_view profile) {
    return file_explorer_labels(locale, file_explorer_resource_catalog(profile));
}
} // namespace file_explorer_demo

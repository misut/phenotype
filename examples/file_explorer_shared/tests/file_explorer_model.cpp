#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

import file_explorer_shared;

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
#if defined(__wasi__) || defined(__ANDROID__)
    std::puts("SKIP: file explorer model filesystem test");
#else
    namespace demo = file_explorer_demo;
    namespace fs = std::filesystem;

    assert(demo::sanitize_file_name("../ bad:name") == "badname.txt");
    assert(demo::sanitize_file_name("  Report  Draft  ") == "Report Draft.txt");
    assert(demo::sanitize_file_name(".hidden") == "hidden.txt");
    assert(demo::sanitize_folder_name("../ Drafts:v1") == "Draftsv1");
    assert(demo::sanitize_folder_name("  Review   Folder  ") == "Review Folder");

    auto catalog = demo::file_explorer_resource_catalog("desktop");
    assert(catalog.default_font_family == "Pretendard");
    assert(demo::file_explorer_labels("ko", "desktop").sidebar_recents
        == "최근 항목");
    assert(demo::file_explorer_labels("ko", catalog).sidebar_recents
        == "최근 항목");
    assert(demo::file_explorer_labels("ja", "desktop").sidebar_recents
        == "Recents");
    assert(demo::file_explorer_labels("ko", "mobile").tab_create == "만들기");
    auto packaged_texts = std::vector<demo::PackageResourceText>{
        {.source = "locales/ko.toml",
         .text = R"(
[app]
sidebar_recents = "패키지 최근 항목"

[actions]
create_file = "패키지 새 파일"
)"},
        {.source = "locales/en.toml",
         .text = R"(
[app]
sidebar_recents = "Packaged Recents"

[actions]
create_file = "Packaged New File"
)"},
    };
    auto packaged_catalog =
        demo::file_explorer_resource_catalog_from_package_texts(
            "desktop",
            R"(
[application]
id = "com.example.file-explorer"
display_name = "Packaged Explorer"
version = "0.1.0"
entry = "file_explorer_desktop"
platforms = ["macos"]

[resources]
default_locale = "ko"
default_font_family = "Pretendard"

[[locales]]
tag = "ko"
source = "locales/ko.toml"
fallback = ["en"]

[[locales]]
tag = "en"
source = "locales/en.toml"
fallback = []
)",
            packaged_texts);
    assert(packaged_catalog.application.display_name == "Packaged Explorer");
    assert(packaged_catalog.default_locale == "ko");
    assert(demo::file_explorer_labels("ko", packaged_catalog).sidebar_recents
        == "패키지 최근 항목");
    assert(demo::file_explorer_labels("ko", packaged_catalog).create_file
        == "패키지 새 파일");
    assert(demo::file_explorer_labels("ja", packaged_catalog).sidebar_recents
        == "패키지 최근 항목");

    auto parsed_location = demo::parse_explorer_input("location:documents");
    assert(parsed_location.ok);
    assert(parsed_location.input.kind == demo::ExplorerInputKind::SelectLocation);
    assert(parsed_location.input.value == "documents");
    auto parsed_open = demo::parse_explorer_input("open:Documents");
    assert(parsed_open.ok);
    assert(parsed_open.input.kind == demo::ExplorerInputKind::OpenEntry);
    assert(parsed_open.input.value == "Documents");
    auto parsed_activate = demo::parse_explorer_input("activate:Documents");
    assert(parsed_activate.ok);
    assert(parsed_activate.input.kind == demo::ExplorerInputKind::ActivateEntry);
    assert(parsed_activate.input.value == "Documents");
    auto parsed_sort = demo::parse_explorer_input("sort:kind");
    assert(parsed_sort.ok);
    assert(parsed_sort.input.kind == demo::ExplorerInputKind::Sort);
    assert(parsed_sort.input.sort_mode == demo::SortMode::Kind);
    auto parsed_recent_sort = demo::parse_explorer_input("sort:recent");
    assert(parsed_recent_sort.ok);
    assert(parsed_recent_sort.input.kind == demo::ExplorerInputKind::Sort);
    assert(parsed_recent_sort.input.sort_mode == demo::SortMode::Recent);
    auto parsed_viewport = demo::parse_explorer_input("viewport:900x620@2");
    assert(parsed_viewport.ok);
    assert(parsed_viewport.input.kind == demo::ExplorerInputKind::Viewport);
    assert(parsed_viewport.input.viewport_width == 900);
    assert(parsed_viewport.input.viewport_height == 620);
    assert(parsed_viewport.input.viewport_scale == 2.0f);
    assert(parsed_viewport.input.value == "900x620@2");
    auto parsed_bad_viewport = demo::parse_explorer_input("viewport:900");
    assert(!parsed_bad_viewport.ok);
    assert(parsed_bad_viewport.error.find("viewport") != std::string::npos);
    auto parsed_bad = demo::parse_explorer_input("sort:date");
    assert(!parsed_bad.ok);
    assert(parsed_bad.error.find("sort") != std::string::npos);
    auto expected_selected =
        demo::parse_explorer_expectation("selected:README.txt");
    assert(expected_selected.ok);
    assert(expected_selected.expectation.kind
        == demo::ExplorerExpectationKind::Selected);
    auto expected_operation =
        demo::parse_explorer_expectation("operation:file_delete:ok");
    assert(expected_operation.ok);
    assert(expected_operation.expectation.expects_operation_ok);
    assert(expected_operation.expectation.operation_ok);
    auto expected_missing =
        demo::parse_explorer_expectation("missing-entry:Ghost.txt");
    assert(expected_missing.ok);
    auto bad_expectation = demo::parse_explorer_expectation("selected");
    assert(!bad_expectation.ok);

    std::string const profile = "test-model-contract";
    auto root = demo::demo_root(profile);
    std::error_code ec;
    fs::remove_all(root, ec);

    auto state = demo::make_state(profile);
    assert(state.viewport_width == demo::k_desktop_default_viewport_width);
    assert(state.viewport_height == demo::k_desktop_default_viewport_height);
    assert(state.viewport_scale == 1.0f);
    auto chrome = demo::explorer_chrome_metrics(state, profile);
    assert(chrome.viewport.width == demo::k_desktop_default_viewport_width);
    assert(chrome.viewport.height == demo::k_desktop_default_viewport_height);
    assert(chrome.icon_grid_columns == 6);
    assert(chrome.icon_grid_visible_rows == 3);
    assert(chrome.icon_grid_visible_capacity == 18);
    assert(chrome.sidebar_row_height == demo::k_desktop_sidebar_row_height);
    assert(chrome.sidebar_heading_height
           == demo::k_desktop_sidebar_heading_height);
    assert(chrome.toolbar_group_height
           == demo::k_desktop_toolbar_group_height);
    assert(chrome.toolbar_group_radius
           == demo::k_desktop_toolbar_group_radius);
    assert(chrome.toolbar_icon_button_width
           == demo::k_desktop_toolbar_icon_button_width);
    assert(chrome.toolbar_icon_button_height
           == demo::k_desktop_toolbar_icon_button_height);
    assert(chrome.toolbar_group_count == 6);
    assert(chrome.toolbar_separator_count == 3);
    assert(chrome.toolbar_icon_button_count == 15);
    assert(chrome.finder_segmented_toolbar);
    assert(chrome.integrated_titlebar);
    assert(chrome.native_window_controls);
    assert(!chrome.duplicate_window_controls);
    assert(demo::explorer_icon_grid_columns(chrome).size() == 6);
    fs::remove_all(demo::demo_root("mobile"), ec);
    auto mobile_state = demo::make_state("mobile");
    auto mobile_chrome = demo::explorer_chrome_metrics(mobile_state, "mobile");
    assert(mobile_state.viewport_width == demo::k_mobile_default_viewport_width);
    assert(mobile_state.viewport_height == demo::k_mobile_default_viewport_height);
    assert(mobile_state.viewport_scale == 2.0f);
    assert(!mobile_chrome.integrated_titlebar);
    assert(!mobile_chrome.native_window_controls);
    assert(mobile_chrome.icon_grid_columns == 0);
    assert(mobile_chrome.toolbar_group_count == 0);
    assert(mobile_chrome.toolbar_separator_count == 0);
    assert(mobile_chrome.toolbar_icon_button_count == 0);
    assert(!mobile_chrome.finder_segmented_toolbar);
    fs::remove_all(mobile_state.root, ec);
    auto snap = demo::snapshot(state);
    assert(!snap.has_selection);
    assert(snap.file_count >= 15);
    assert(snap.folder_count == 3);
    assert(!demo::deletable_directory(state.root, demo::trash_path(state.root)));
    assert(snap.can_create_file);
    assert(snap.can_create_folder);
    assert(!snap.can_delete_selected);
    assert(!snap.can_duplicate_selected);
    assert(!snap.can_preview_selected);
    assert(snap.action_summary.find("Select a file") != std::string::npos);
    assert(snap.sort_label == "Sort: Recent");
    assert(snap.entries.size() >= 5);
    assert(snap.entries[0].name == "작성예시3_선택_DC 탈퇴신청서.pdf");
    assert(snap.entries[1].name == "작성예시2_필수_운용지시서.pdf");
    assert(snap.entries[2].name == "작성예시1_필수_중도인출 신청서.pdf");
    assert(snap.entries[3].name == "2_필수_운용지시서.pdf");
    assert(snap.entries[4].name == "1_필수_중도인출 신청서.pdf");

    demo::select_entry(state, "README.txt");
    snap = demo::snapshot(state);
    assert(snap.has_selection);
    assert(snap.selected.name == "README.txt");
    assert(snap.can_delete_selected);
    assert(snap.can_duplicate_selected);
    assert(snap.can_preview_selected);
    assert(snap.selected_kind_label == "TXT File");
    assert(snap.selected_size_label != "--");
    assert(snap.action_summary.find("Selected README.txt") != std::string::npos);
    demo::set_sort_mode(state, demo::SortMode::Name);
    snap = demo::snapshot(state);
    bool saw_file_after_folder = false;
    for (auto const& entry : snap.entries) {
        if (!entry.folder)
            saw_file_after_folder = true;
        assert(!entry.folder || !saw_file_after_folder);
    }
    bool saw_korean_pdf = false;
    bool saw_japanese_pdf = false;
    bool saw_chinese_pdf = false;
    for (auto const& entry : snap.entries) {
        if (entry.name == "1_필수_중도인출 신청서.pdf")
            saw_korean_pdf = true;
        if (entry.name == "작성예시1_필수_중도인출 신청서.pdf")
            saw_korean_pdf = true;
        if (entry.name == "契約書_サンプル.pdf")
            saw_japanese_pdf = true;
        if (entry.name == "资产证明.pdf")
            saw_chinese_pdf = true;
    }
    assert(saw_korean_pdf);
    assert(saw_japanese_pdf);
    assert(saw_chinese_pdf);

    state.draft_name = "../ Launch Plan";
    state.draft_body = "Created from test_file_explorer_model.";
    demo::create_file(state);
    assert(state.selected_name == "Launch Plan.txt");
    assert(fs::exists(state.current / "Launch Plan.txt"));
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_create ok - Launch Plan.txt") != std::string::npos);
    assert(demo::read_preview(state.current / "Launch Plan.txt")
        .find("Created from test_file_explorer_model") != std::string::npos);

    demo::delete_selected(state);
    assert(!fs::exists(state.current / "Launch Plan.txt"));
    assert(fs::exists(demo::trash_path(state.root) / "Launch Plan.txt"));
    assert(state.selected_name.empty());
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_delete ok - Launch Plan.txt") != std::string::npos);

    state.draft_folder_name = "../ Review Folder";
    demo::create_folder(state);
    assert(state.selected_name == "Review Folder");
    assert(fs::is_directory(state.current / "Review Folder"));
    snap = demo::snapshot(state);
    assert(snap.selected.folder);
    assert(snap.can_delete_selected);
    assert(snap.operation_label
        .find("Operation: folder_create ok - Review Folder") != std::string::npos);
    assert(snap.preview.find("Open this folder") != std::string::npos);

    demo::select_entry(state, "Review Folder");
    assert(demo::same_path(state.current, state.root));
    snap = demo::snapshot(state);
    assert(snap.has_selection);
    assert(snap.selected.folder);
    assert(snap.operation_label
        .find("Operation: folder_select ok - Review Folder") != std::string::npos);

    demo::activate_entry(state, "Review Folder");
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Review Folder");
    assert(state.selected_name.empty());
    demo::go_back(state);
    assert(demo::same_path(state.current, state.root));
    demo::select_entry(state, "Review Folder");

    demo::write_file_if_missing(
        state.current / state.selected_name / "Nested Note.txt",
        "Nested file proves folder deletion moves contents to Trash.\n");
    snap = demo::snapshot(state);
    assert(snap.can_delete_selected);

    demo::delete_selected(state);
    assert(!fs::exists(state.current / "Review Folder"));
    assert(fs::is_directory(demo::trash_path(state.root) / "Review Folder"));
    assert(fs::exists(demo::trash_path(state.root)
        / "Review Folder" / "Nested Note.txt"));
    assert(state.selected_name.empty());
    assert(demo::snapshot(state).operation_label
        .find("Operation: folder_delete ok - Review Folder") != std::string::npos);

    demo::apply_startup_scenario(state, "created-preview");
    assert(state.selected_name == "Action Note.txt");
    assert(fs::exists(state.current / "Action Note.txt"));
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_create ok - Action Note.txt") != std::string::npos);
    assert(demo::read_preview(state.current / "Action Note.txt")
        .find("Created from artifact scenario") != std::string::npos);

    demo::apply_startup_scenario(state, "created-preview");
    assert(state.selected_name == "Action Note.txt");
    assert(fs::exists(state.current / "Action Note.txt"));
    assert(!fs::exists(state.current / "Action Note 2.txt"));

    demo::apply_startup_scenario(state, "deleted-file");
    assert(!fs::exists(state.current / "Delete Me.txt"));
    assert(fs::exists(demo::trash_path(state.root) / "Delete Me.txt"));
    assert(state.selected_name.empty());
    assert(state.status == "Moved Delete Me.txt to Trash");
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_delete ok - Delete Me.txt") != std::string::npos);

    demo::apply_startup_scenario(state, "trash-view");
    assert(demo::relative_location(state.root, state.current) == "Trash");
    assert(fs::exists(demo::trash_path(state.root) / "Trash Note.txt"));
    snap = demo::snapshot(state);
    bool saw_trash_note = false;
    for (auto const& entry : snap.entries) {
        if (entry.name == "Trash Note.txt")
            saw_trash_note = true;
    }
    assert(saw_trash_note);
    assert(snap.operation_label
        .find("Operation: file_delete ok - Trash Note.txt") != std::string::npos);
    demo::select_entry(state, "Trash Note.txt");
    demo::delete_selected(state);
    assert(!fs::exists(demo::trash_path(state.root) / "Trash Note.txt"));
    assert(state.status == "Deleted Trash Note.txt from Trash");

    demo::apply_startup_scenario(state, "created-folder");
    assert(state.selected_name == "Review Folder");
    assert(fs::is_directory(state.current / "Review Folder"));
    assert(demo::snapshot(state).operation_label
        .find("Operation: folder_create ok - Review Folder") != std::string::npos);

    demo::apply_startup_scenario(state, "deleted-folder");
    assert(!fs::exists(state.current / "Trash Folder"));
    assert(fs::is_directory(demo::trash_path(state.root) / "Trash Folder"));
    assert(fs::exists(demo::trash_path(state.root)
        / "Trash Folder" / "Nested Note.txt"));
    assert(state.selected_name.empty());
    assert(demo::snapshot(state).operation_label
        .find("Operation: folder_delete ok - Trash Folder") != std::string::npos);

    demo::select_entry(state, "README.txt");
    assert(demo::snapshot(state).operation_label
        .find("Operation: file_read ok - README.txt") != std::string::npos);
    demo::duplicate_selected(state);
    assert(state.selected_name == "README copy.txt");
    assert(fs::exists(state.current / "README copy.txt"));
    assert(state.status == "Duplicated README.txt to README copy.txt");
    snap = demo::snapshot(state);
    assert(snap.selected.name == "README copy.txt");
    assert(snap.operation_label
        .find("Operation: file_duplicate ok - README copy.txt") != std::string::npos);
    assert(snap.can_delete_selected);
    assert(snap.can_duplicate_selected);
    assert(snap.selected_path_label.find("README copy.txt") != std::string::npos);

    demo::set_sort_mode(state, demo::SortMode::Kind);
    snap = demo::snapshot(state);
    assert(snap.sort_label == "Sort: Kind");
    assert(state.status == "Sorted by Kind");
    std::string previous_kind;
    for (auto const& entry : snap.entries) {
        if (entry.folder)
            continue;
        auto current_kind = demo::lower_copy(demo::entry_kind_label(entry));
        assert(previous_kind.empty() || previous_kind <= current_kind);
        previous_kind = std::move(current_kind);
    }
    demo::cycle_sort_mode(state);
    snap = demo::snapshot(state);
    assert(snap.sort_label == "Sort: Size");
    std::uintmax_t previous_size = 0;
    bool saw_sized_file = false;
    for (auto const& entry : snap.entries) {
        if (entry.folder)
            continue;
        if (saw_sized_file)
            assert(previous_size >= entry.size);
        previous_size = entry.size;
        saw_sized_file = true;
    }
    demo::cycle_sort_mode(state);
    assert(demo::snapshot(state).sort_label == "Sort: Name");

    demo::apply_startup_scenario(state, "documents-preview");
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Documents");
    assert(state.selected_name == "Project Notes.txt");
    assert(demo::snapshot(state).preview.find("Finder-like desktop layout")
        != std::string::npos);
    assert(demo::snapshot(state).can_go_back);

    demo::go_back(state);
    assert(demo::same_path(state.current, state.root));
    assert(state.status == "Went back to Demo Root");
    assert(demo::snapshot(state).can_go_forward);

    demo::go_forward(state);
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Documents");
    assert(state.status == "Went forward to Demo Root/Documents");
    assert(demo::snapshot(state).can_go_back);

    demo::select_location(state, "documents");
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Documents");
    demo::go_up(state);
    assert(demo::same_path(state.current, state.root));
    assert(state.forward_stack.empty());

    demo::apply_startup_scenario(state, "history-forward");
    assert(demo::relative_location(state.root, state.current)
        == "Demo Root/Documents");
    assert(state.status == "Went forward to Demo Root/Documents");

    demo::apply_startup_scenario(state, "sorted-kind");
    assert(demo::snapshot(state).sort_label == "Sort: Kind");
    assert(state.status == "Sorted by Kind");

    demo::apply_startup_scenario(state, "search-active");
    snap = demo::snapshot(state);
    assert(state.search == "Screen");
    assert(state.status == "Searching for Screen");
    assert(!snap.has_selection);
    assert(!snap.entries.empty());
    for (auto const& entry : snap.entries) {
        assert(demo::lower_copy(entry.name).find("screen") != std::string::npos);
    }

    demo::apply_startup_scenario(state, "duplicated-file");
    assert(state.selected_name == "README copy.txt");
    assert(fs::exists(state.current / "README copy.txt"));
    assert(state.status == "Duplicated README.txt to README copy.txt");

    std::string const drive_profile = "test-cli-drive";
    auto drive_root = demo::demo_root(drive_profile);
    fs::remove_all(drive_root, ec);
    std::vector<demo::ExplorerInput> inputs{
        {.kind = demo::ExplorerInputKind::Viewport,
         .value = "900x620@2",
         .viewport_width = 900,
         .viewport_height = 620,
         .viewport_scale = 2.0f},
        {.kind = demo::ExplorerInputKind::DraftName, .value = "CLI Note"},
        {.kind = demo::ExplorerInputKind::DraftBody,
         .value = "Created from typed CLI input."},
        {.kind = demo::ExplorerInputKind::CreateFile},
        {.kind = demo::ExplorerInputKind::DuplicateSelected},
        {.kind = demo::ExplorerInputKind::DeleteSelected},
        {.kind = demo::ExplorerInputKind::SelectLocation, .value = "trash"},
        {.kind = demo::ExplorerInputKind::Sort,
         .value = "kind",
         .sort_mode = demo::SortMode::Kind},
    };
    auto driven = demo::drive_explorer(drive_profile, inputs);
    assert(driven.profile == drive_profile);
    assert(driven.trace.size() == inputs.size());
    assert(driven.state.viewport_width == 900);
    assert(driven.state.viewport_height == 620);
    assert(driven.state.viewport_scale == 2.0f);
    assert(driven.chrome.viewport.width == 900);
    assert(driven.chrome.viewport.height == 620);
    assert(driven.chrome.viewport.scale == 2.0f);
    assert(driven.chrome.icon_grid_columns == 3);
    assert(driven.trace[0].chrome.viewport.width == 900);
    assert(driven.trace[0].chrome.icon_grid_columns == 3);
    assert(driven.trace[0].status == "Viewport set to 900x620@2");
    assert(driven.trace[3].operation.kind == "file_create");
    assert(driven.trace[3].operation.ok);
    assert(driven.trace[4].operation.kind == "file_duplicate");
    assert(driven.trace[4].operation.ok);
    assert(driven.trace[5].operation.kind == "file_delete");
    assert(driven.trace[5].operation.ok);
    assert(driven.snapshot.relative_location == "Trash");
    assert(driven.snapshot.sort_label == "Sort: Kind");
    bool saw_cli_note_copy = false;
    for (auto const& entry : driven.snapshot.entries) {
        if (entry.name == "CLI Note copy.txt")
            saw_cli_note_copy = true;
    }
    assert(saw_cli_note_copy);
    std::vector<demo::ExplorerExpectation> expectations{
        {.kind = demo::ExplorerExpectationKind::Location, .value = "Trash"},
        {.kind = demo::ExplorerExpectationKind::Entry,
         .value = "CLI Note copy.txt"},
        {.kind = demo::ExplorerExpectationKind::MissingEntry,
         .value = "CLI Note.txt"},
        {.kind = demo::ExplorerExpectationKind::Operation,
         .value = "file_delete",
         .expects_operation_ok = true,
         .operation_ok = true},
        {.kind = demo::ExplorerExpectationKind::StatusContains,
         .value = "Sorted"},
    };
    auto checked = demo::check_explorer_expectations(driven, expectations);
    assert(checked.size() == expectations.size());
    assert(demo::explorer_expectations_ok(checked));
    assert(checked[0].actual == "Trash");
    assert(checked[1].actual == "CLI Note copy.txt");
    assert(checked[2].actual == "<missing>");
    auto failed_expectation = demo::check_explorer_expectation(
        driven,
        {.kind = demo::ExplorerExpectationKind::Selected,
         .value = "CLI Note.txt"});
    assert(!failed_expectation.ok);
    assert(failed_expectation.actual == "<none>");
    fs::remove_all(drive_root, ec);

    std::string const navigation_profile = "test-cli-navigation";
    auto navigation_root = demo::demo_root(navigation_profile);
    fs::remove_all(navigation_root, ec);
    std::vector<demo::ExplorerInput> open_inputs{
        {.kind = demo::ExplorerInputKind::OpenEntry, .value = "Documents"},
        {.kind = demo::ExplorerInputKind::SelectEntry,
         .value = "Project Notes.txt"},
    };
    auto opened = demo::drive_explorer(
        navigation_profile,
        open_inputs);
    assert(opened.snapshot.relative_location == "Demo Root/Documents");
    assert(opened.snapshot.has_selection);
    assert(opened.snapshot.selected.name == "Project Notes.txt");
    assert(opened.state.last_operation.kind == "file_read");

    fs::remove_all(navigation_root, ec);
    std::vector<demo::ExplorerInput> activate_inputs{
        {.kind = demo::ExplorerInputKind::ActivateEntry, .value = "Documents"},
        {.kind = demo::ExplorerInputKind::ActivateEntry, .value = "Documents"},
    };
    auto activated = demo::drive_explorer(
        navigation_profile,
        activate_inputs);
    assert(activated.trace.size() == 2);
    assert(activated.trace[0].selected_name == "Documents");
    assert(activated.trace[0].operation.kind == "folder_select");
    assert(activated.trace[1].operation.kind == "folder_open");
    assert(activated.trace[1].operation.ok);
    assert(activated.snapshot.relative_location == "Demo Root/Documents");
    fs::remove_all(navigation_root, ec);

    demo::reset_demo_tree(state, profile);
    assert(!demo::snapshot(state).can_go_back);
    assert(!demo::snapshot(state).can_go_forward);
    assert(demo::snapshot(state).operation_label.empty());

    fs::remove_all(root, ec);
    std::puts("PASS: file explorer model contract");
#endif
}

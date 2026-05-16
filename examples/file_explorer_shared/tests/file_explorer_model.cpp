#include <cassert>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>

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
    assert(demo::file_explorer_labels("ja", "desktop").sidebar_recents
        == "Recents");
    assert(demo::file_explorer_labels("ko", "mobile").tab_create == "만들기");

    std::string const profile = "test-model-contract";
    auto root = demo::demo_root(profile);
    std::error_code ec;
    fs::remove_all(root, ec);

    auto state = demo::make_state(profile);
    auto snap = demo::snapshot(state);
    assert(snap.has_selection);
    assert(snap.selected.name == "README.txt");
    assert(snap.file_count >= 15);
    assert(snap.folder_count == 3);
    assert(!demo::deletable_directory(state.root, demo::trash_path(state.root)));
    assert(snap.can_create_file);
    assert(snap.can_create_folder);
    assert(snap.can_delete_selected);
    assert(snap.can_duplicate_selected);
    assert(snap.can_preview_selected);
    assert(snap.selected_kind_label == "TXT File");
    assert(snap.selected_size_label != "--");
    assert(snap.action_summary.find("Selected README.txt") != std::string::npos);
    assert(snap.sort_label == "Sort: Name");
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

    demo::reset_demo_tree(state, profile);
    assert(!demo::snapshot(state).can_go_back);
    assert(!demo::snapshot(state).can_go_forward);
    assert(demo::snapshot(state).operation_label.empty());

    fs::remove_all(root, ec);
    std::puts("PASS: file explorer model contract");
#endif
}

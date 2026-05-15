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

    state.draft_name = "../ Launch Plan";
    state.draft_body = "Created from test_file_explorer_model.";
    demo::create_file(state);
    assert(state.selected_name == "Launch Plan.txt");
    assert(fs::exists(state.current / "Launch Plan.txt"));
    assert(demo::read_preview(state.current / "Launch Plan.txt")
        .find("Created from test_file_explorer_model") != std::string::npos);

    demo::delete_selected(state);
    assert(!fs::exists(state.current / "Launch Plan.txt"));
    assert(state.selected_name.empty());

    demo::apply_startup_scenario(state, "created-preview");
    assert(state.selected_name == "Action Note.txt");
    assert(fs::exists(state.current / "Action Note.txt"));
    assert(demo::read_preview(state.current / "Action Note.txt")
        .find("Created from artifact scenario") != std::string::npos);

    demo::apply_startup_scenario(state, "created-preview");
    assert(state.selected_name == "Action Note.txt");
    assert(fs::exists(state.current / "Action Note.txt"));
    assert(!fs::exists(state.current / "Action Note 2.txt"));

    demo::apply_startup_scenario(state, "deleted-file");
    assert(!fs::exists(state.current / "Delete Me.txt"));
    assert(state.selected_name.empty());
    assert(state.status == "Deleted Delete Me.txt");

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

    demo::reset_demo_tree(state, profile);
    assert(!demo::snapshot(state).can_go_back);
    assert(!demo::snapshot(state).can_go_forward);

    fs::remove_all(root, ec);
    std::puts("PASS: file explorer model contract");
#endif
}

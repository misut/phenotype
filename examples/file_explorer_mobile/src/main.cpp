#include <concepts>
#include <cstdlib>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

import file_explorer_shared;
import phenotype;
import phenotype.native;

namespace {

struct SelectLocation { std::string id; };
struct SelectEntry { std::string name; };
struct SearchChanged { std::string text; };
struct DraftNameChanged { std::string text; };
struct DraftFolderNameChanged { std::string text; };
struct DraftBodyChanged { std::string text; };
struct SelectTab { std::size_t value; };
struct CreateFile {};
struct CreateFolder {};
struct DeleteSelected {};
struct DuplicateSelected {};
struct GoBack {};
struct GoForward {};
struct GoUp {};
struct CycleSort {};
struct ResetDemo {};
struct Resized { int width; int height; float scale; };

using Msg = std::variant<
    SelectLocation,
    SelectEntry,
    SearchChanged,
    DraftNameChanged,
    DraftFolderNameChanged,
    DraftBodyChanged,
    SelectTab,
    CreateFile,
    CreateFolder,
    DeleteSelected,
    DuplicateSelected,
    GoBack,
    GoForward,
    GoUp,
    CycleSort,
    ResetDemo,
    Resized>;

file_explorer_demo::ExplorerState initial_explorer_state() {
    auto state = file_explorer_demo::make_state("mobile");
    if (char const* raw = std::getenv("PHENOTYPE_FILE_EXPLORER_SCENARIO")) {
        file_explorer_demo::apply_startup_scenario(state, raw);
    }
    return state;
}

struct State {
    file_explorer_demo::ExplorerState explorer = initial_explorer_state();
};

Msg on_search_changed(std::string text) {
    return SearchChanged{std::move(text)};
}

Msg on_draft_name_changed(std::string text) {
    return DraftNameChanged{std::move(text)};
}

Msg on_draft_folder_name_changed(std::string text) {
    return DraftFolderNameChanged{std::move(text)};
}

Msg on_draft_body_changed(std::string text) {
    return DraftBodyChanged{std::move(text)};
}

void update(State& state, Msg msg) {
    auto& explorer = state.explorer;
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, SelectLocation>) {
            file_explorer_demo::select_location(explorer, m.id);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, SelectEntry>) {
            file_explorer_demo::select_entry(explorer, m.name);
            if (!explorer.selected_name.empty())
                explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, SearchChanged>) {
            file_explorer_demo::set_search_filter(explorer, m.text);
        } else if constexpr (std::same_as<T, DraftNameChanged>) {
            explorer.draft_name = m.text;
        } else if constexpr (std::same_as<T, DraftFolderNameChanged>) {
            explorer.draft_folder_name = m.text;
        } else if constexpr (std::same_as<T, DraftBodyChanged>) {
            explorer.draft_body = m.text;
        } else if constexpr (std::same_as<T, SelectTab>) {
            explorer.mobile_tab = m.value;
        } else if constexpr (std::same_as<T, CreateFile>) {
            file_explorer_demo::create_file(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, CreateFolder>) {
            file_explorer_demo::create_folder(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, DeleteSelected>) {
            file_explorer_demo::delete_selected(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, DuplicateSelected>) {
            file_explorer_demo::duplicate_selected(explorer);
            explorer.mobile_tab = 1;
        } else if constexpr (std::same_as<T, GoBack>) {
            file_explorer_demo::go_back(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, GoForward>) {
            file_explorer_demo::go_forward(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, GoUp>) {
            file_explorer_demo::go_up(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, CycleSort>) {
            file_explorer_demo::cycle_sort_mode(explorer);
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, ResetDemo>) {
            file_explorer_demo::reset_demo_tree(explorer, "mobile");
            explorer.mobile_tab = 0;
        } else if constexpr (std::same_as<T, Resized>) {
            explorer.viewport_width = m.width;
            explorer.viewport_height = m.height;
            explorer.viewport_scale = m.scale;
        }
    }, msg);
}

void top_bar(State const& state, file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::material_surface(layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Clear,
        .role = MaterialSurfaceRole::Toolbar,
        .padding = SpaceToken::Md,
        .gap = SpaceToken::Sm,
        .semantic_label = "Toolbar",
    }, [&] {
        widget::text("Files");
        widget::text(snap.relative_location, TextSize::Small, TextColor::Muted);
        layout::spacer(8);
        widget::text_field<Msg>("Search", explorer.search, on_search_changed);
        layout::spacer(8);
        std::vector<phenotype::str> tabs;
        tabs.emplace_back("Browse", 6u);
        tabs.emplace_back("Preview", 7u);
        tabs.emplace_back("Create", 6u);
        widget::tabs<Msg>(tabs, explorer.mobile_tab, [](std::size_t index) -> Msg {
            return SelectTab{index};
        });
    });
}

void location_strip() {
    using namespace phenotype;
    layout::material_surface(layout::MaterialSurfaceOptions{
        .kind = MaterialKind::Thin,
        .role = MaterialSurfaceRole::Navigation,
        .direction = FlexDirection::Row,
        .padding = SpaceToken::Sm,
        .gap = SpaceToken::Xs,
        .cross_align = CrossAxisAlignment::Center,
        .semantic_label = "Locations",
    }, [&] {
        layout::row([&] {
            widget::button<Msg>("Root", SelectLocation{"root"});
            widget::button<Msg>("Docs", SelectLocation{"documents"});
            widget::button<Msg>("Pics", SelectLocation{"pictures"});
            widget::button<Msg>("Shared", SelectLocation{"shared"});
        }, SpaceToken::Xs, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    });
}

void browse_tab(file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    layout::material_surface(MaterialKind::Regular, [&] {
        layout::row([&] {
            layout::weighted(1.0f, [&] {
                std::string summary = std::to_string(snap.entries.size()) + " items";
                if (!snap.sort_label.empty())
                    summary += " - " + snap.sort_label;
                widget::text(summary, TextSize::Small, TextColor::Muted);
            });
            widget::button<Msg>("Sort", CycleSort{});
            widget::button<Msg>("Back", GoBack{},
                                ButtonVariant::Default,
                                !snap.can_go_back);
            widget::button<Msg>("Forward", GoForward{},
                                ButtonVariant::Default,
                                !snap.can_go_forward);
            widget::button<Msg>("Up", GoUp{});
        });
        layout::spacer(8);
        layout::scroll_view(430.0f, [&] {
            if (snap.entries.empty()) {
                widget::text("No matching files.");
                return;
            }
            for (auto const& entry : snap.entries) {
                layout::material_surface(
                    entry.folder ? MaterialKind::Clear : MaterialKind::Thin,
                    [&] {
                        widget::button<Msg>(
                            file_explorer_demo::entry_label(entry),
                            SelectEntry{entry.name},
                            entry.folder
                                ? ButtonVariant::Default
                                : ButtonVariant::Primary);
                        widget::text(
                            entry.folder
                                ? "Folder"
                                : file_explorer_demo::format_size(entry.size),
                            TextSize::Small,
                            TextColor::Muted);
                    },
                    SpaceToken::Sm,
                    SpaceToken::Xs);
            }
        }, SpaceToken::Sm);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void preview_tab(
        State const& state,
        file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::material_surface(MaterialKind::Thick, [&] {
        widget::text("Preview");
        layout::spacer(4);
        if (snap.has_selection) {
            widget::text(snap.selected.name);
            widget::text(snap.selected_kind_label + " - "
                             + snap.selected_size_label,
                         TextSize::Small,
                         TextColor::Muted);
        } else {
            widget::text("Select a file from Browse.");
        }
        layout::spacer(8);
        widget::code(snap.preview);
        layout::spacer(12);
        widget::button<Msg>("Duplicate File",
                            DuplicateSelected{},
                            ButtonVariant::Default,
                            !snap.can_duplicate_selected);
        layout::spacer(8);
        widget::button<Msg>("Delete Selected",
                            DeleteSelected{},
                            ButtonVariant::Default,
                            !snap.can_delete_selected);
        layout::spacer(8);
        widget::text(explorer.status, TextSize::Small, TextColor::Muted);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void create_tab(State const& state) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::material_surface(MaterialKind::Regular, [&] {
        widget::text("Create");
        widget::text("Files and folders are written only inside the demo root.",
                     TextSize::Small,
                     TextColor::Muted);
        layout::spacer(8);
        widget::text_field<Msg>("File name", explorer.draft_name, on_draft_name_changed);
        layout::spacer(8);
        widget::text_field<Msg>("Contents", explorer.draft_body, on_draft_body_changed);
        layout::spacer(10);
        widget::button<Msg>("Create File", CreateFile{}, ButtonVariant::Primary);
        layout::spacer(10);
        widget::text_field<Msg>(
            "Folder name",
            explorer.draft_folder_name,
            on_draft_folder_name_changed);
        layout::spacer(8);
        widget::button<Msg>("Create Folder", CreateFolder{}, ButtonVariant::Default);
        layout::spacer(6);
        widget::button<Msg>("Reset Demo Files", ResetDemo{});
    }, SpaceToken::Md, SpaceToken::Sm);
}

void view(State const& state) {
    using namespace phenotype;
    auto snap = file_explorer_demo::snapshot(state.explorer);
    layout::padded(SpaceToken::Md, [&] {
        layout::material_container(
            layout::MaterialContainerOptions{
                .container_id = 3100u,
                .spacing = 12.0f,
                .morph_transitions = true,
            },
            [&] {
                layout::column([&] {
                    top_bar(state, snap);
                    location_strip();
                    if (state.explorer.mobile_tab == 0) {
                        browse_tab(snap);
                    } else if (state.explorer.mobile_tab == 1) {
                        preview_tab(state, snap);
                    } else {
                        create_tab(state);
                    }
                    layout::status_bar([&] {
                        widget::text("Status");
                        std::string detail = "Status: " + state.explorer.status;
                        if (!snap.operation_label.empty()) {
                            detail += "\n";
                            detail += snap.operation_label;
                        }
                        detail += "\nViewport: ";
                        detail += std::to_string(state.explorer.viewport_width);
                        detail += " x ";
                        detail += std::to_string(state.explorer.viewport_height);
                        widget::code(detail);
                    }, MaterialKind::Thick);
                }, SpaceToken::Sm);
            });
    });
}

} // namespace

int main() {
    return phenotype::native::run_app<State, Msg>(
        390,
        780,
        "phenotype file explorer mobile",
        view,
        update,
        [](int width, int height, float scale) -> Msg {
            return Resized{width, height, scale};
        });
}

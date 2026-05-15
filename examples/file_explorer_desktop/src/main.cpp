#include <concepts>
#include <cstddef>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

#include "../../file_explorer_shared/file_explorer_model.hpp"

import phenotype;
import phenotype.native;

namespace {

struct SelectLocation { std::string id; };
struct SelectEntry { std::string name; };
struct SearchChanged { std::string text; };
struct DraftNameChanged { std::string text; };
struct DraftBodyChanged { std::string text; };
struct CreateFile {};
struct DeleteSelected {};
struct GoUp {};
struct Refresh {};
struct ResetDemo {};
struct Resized { int width; int height; float scale; };

using Msg = std::variant<
    SelectLocation,
    SelectEntry,
    SearchChanged,
    DraftNameChanged,
    DraftBodyChanged,
    CreateFile,
    DeleteSelected,
    GoUp,
    Refresh,
    ResetDemo,
    Resized>;

struct State {
    file_explorer_demo::ExplorerState explorer =
        file_explorer_demo::make_state("desktop");
};

Msg on_search_changed(std::string text) {
    return SearchChanged{std::move(text)};
}

Msg on_draft_name_changed(std::string text) {
    return DraftNameChanged{std::move(text)};
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
        } else if constexpr (std::same_as<T, SelectEntry>) {
            file_explorer_demo::select_entry(explorer, m.name);
        } else if constexpr (std::same_as<T, SearchChanged>) {
            explorer.search = m.text;
            explorer.status = explorer.search.empty()
                ? "Filter cleared."
                : "Filtering by " + explorer.search;
        } else if constexpr (std::same_as<T, DraftNameChanged>) {
            explorer.draft_name = m.text;
        } else if constexpr (std::same_as<T, DraftBodyChanged>) {
            explorer.draft_body = m.text;
        } else if constexpr (std::same_as<T, CreateFile>) {
            file_explorer_demo::create_file(explorer);
        } else if constexpr (std::same_as<T, DeleteSelected>) {
            file_explorer_demo::delete_selected(explorer);
        } else if constexpr (std::same_as<T, GoUp>) {
            file_explorer_demo::go_up(explorer);
        } else if constexpr (std::same_as<T, Refresh>) {
            explorer.status = "Refreshed " + file_explorer_demo::relative_location(
                explorer.root,
                explorer.current);
        } else if constexpr (std::same_as<T, ResetDemo>) {
            file_explorer_demo::reset_demo_tree(explorer, "desktop");
        } else if constexpr (std::same_as<T, Resized>) {
            explorer.viewport_width = m.width;
            explorer.viewport_height = m.height;
            explorer.viewport_scale = m.scale;
        }
    }, msg);
}

void sidebar(State const& state) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::sized_box(216.0f, [&] {
        layout::material_surface(MaterialKind::Thin, [&] {
            widget::text("Locations");
            layout::spacer(6);
            widget::button<Msg>("Demo Root", SelectLocation{"root"});
            widget::button<Msg>("Documents", SelectLocation{"documents"});
            widget::button<Msg>("Pictures", SelectLocation{"pictures"});
            widget::button<Msg>("Shared", SelectLocation{"shared"});
            layout::spacer(10);
            layout::divider();
            layout::spacer(10);
            widget::text("Status", TextSize::Small, TextColor::Muted);
            widget::code(explorer.status);
        }, SpaceToken::Md, SpaceToken::Sm);
    });
}

void toolbar(State const& state, file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::material_surface(MaterialKind::Clear, [&] {
        layout::row([&] {
            layout::weighted(1.0f, [&] {
                widget::text("Phenotype Finder");
                widget::text(snap.relative_location, TextSize::Small, TextColor::Muted);
            });
            layout::sized_box(240.0f, [&] {
                widget::text_field<Msg>("Search files", explorer.search, on_search_changed);
            });
            widget::button<Msg>("Up", GoUp{});
            widget::button<Msg>("Refresh", Refresh{});
            widget::button<Msg>("Delete", DeleteSelected{});
            widget::button<Msg>("Reset", ResetDemo{});
        }, SpaceToken::Sm, CrossAxisAlignment::Center, MainAxisAlignment::Start);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void file_table(file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    layout::material_surface(MaterialKind::Regular, [&] {
        layout::row([&] {
            layout::weighted(1.0f, [&] {
                widget::text("Files");
                std::string summary = std::to_string(snap.folder_count) + " folders, "
                    + std::to_string(snap.file_count) + " files";
                widget::text(summary, TextSize::Small, TextColor::Muted);
            });
        });
        layout::spacer(6);
        layout::grid({360.0f, 96.0f, 88.0f}, 28.0f, [&] {
            widget::text("Name", TextSize::Small, TextColor::Muted);
            widget::text("Kind", TextSize::Small, TextColor::Muted);
            widget::text("Size", TextSize::Small, TextColor::Muted);
        });
        layout::divider();
        layout::scroll_view(420.0f, [&] {
            if (snap.entries.empty()) {
                widget::text("No matching files.");
                return;
            }
            for (auto const& entry : snap.entries) {
                layout::grid({360.0f, 96.0f, 88.0f}, 38.0f, [&] {
                    widget::button<Msg>(
                        file_explorer_demo::entry_label(entry),
                        SelectEntry{entry.name},
                        entry.folder ? ButtonVariant::Default : ButtonVariant::Primary);
                    widget::text(std::string(entry.folder ? "Folder" : "Text file"));
                    widget::text(entry.folder ? "-" : file_explorer_demo::format_size(entry.size));
                }, 6.0f);
            }
        }, SpaceToken::Sm);
    }, SpaceToken::Md, SpaceToken::Sm);
}

void preview_panel(State const& state, file_explorer_demo::Snapshot const& snap) {
    using namespace phenotype;
    auto const& explorer = state.explorer;
    layout::sized_box(300.0f, [&] {
        layout::material_surface(MaterialKind::Thick, [&] {
            widget::text("Preview");
            layout::spacer(4);
            if (snap.has_selection) {
                widget::text(snap.selected.name);
                widget::text(file_explorer_demo::format_size(snap.selected.size),
                             TextSize::Small,
                             TextColor::Muted);
            } else {
                widget::text("No file selected", TextSize::Small, TextColor::Muted);
            }
            layout::spacer(8);
            widget::code(snap.preview);
            layout::spacer(12);
            layout::divider();
            layout::spacer(12);
            widget::text("Create File");
            layout::spacer(4);
            widget::text_field<Msg>("File name", explorer.draft_name, on_draft_name_changed);
            layout::spacer(6);
            widget::text_field<Msg>("File contents", explorer.draft_body, on_draft_body_changed);
            layout::spacer(8);
            widget::button<Msg>("Create", CreateFile{}, ButtonVariant::Primary);
        }, SpaceToken::Md, SpaceToken::Sm);
    });
}

void view(State const& state) {
    using namespace phenotype;
    auto snap = file_explorer_demo::snapshot(state.explorer);
    layout::padded(SpaceToken::Lg, [&] {
        layout::column([&] {
            toolbar(state, snap);
            layout::row([&] {
                sidebar(state);
                layout::weighted(1.0f, [&] {
                    file_table(snap);
                });
                preview_panel(state, snap);
            }, SpaceToken::Md, CrossAxisAlignment::Start, MainAxisAlignment::Start);
            layout::material_surface(MaterialKind::Clear, [&] {
                std::string detail = "Demo root: ";
                detail += state.explorer.root.string();
                detail += "\nViewport: ";
                detail += std::to_string(state.explorer.viewport_width);
                detail += " x ";
                detail += std::to_string(state.explorer.viewport_height);
                widget::code(detail);
            }, SpaceToken::Sm, SpaceToken::Xs);
        }, SpaceToken::Md);
    });
}

} // namespace

int main() {
    return phenotype::native::run_app<State, Msg>(
        1040,
        720,
        "phenotype file explorer desktop",
        view,
        update,
        [](int width, int height, float scale) -> Msg {
            return Resized{width, height, scale};
        });
}

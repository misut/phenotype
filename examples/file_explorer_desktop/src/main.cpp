#include <string>
#include <utility>
#include <vector>

import phenotype;
import phenotype.native;

namespace ui = phenotype::ui;

struct ExplorerEntry {
    unsigned int id = 0;
    std::string name;
    std::string kind;
    std::string detail;
};

static phenotype::str label(std::string_view value) {
    return phenotype::str{
        value.data(),
        static_cast<unsigned int>(value.size())};
}

static std::vector<ExplorerEntry> entries() {
    return {
        {1, "Design Brief.pdf", "Document", "Updated today"},
        {2, "Prototype.mov", "Video", "128 MB"},
        {3, "Launch Photos", "Folder", "24 items"},
        {4, "Roadmap.md", "Document", "Pinned"},
        {5, "Screenshots", "Folder", "42 items"},
    };
}

struct FileExplorerDesktopApp {
    ui::View body(ui::Context& cx) const {
        auto query = cx.state<std::string>("query", {});
        auto selected = cx.state<std::size_t>("selected", 0);
        auto mode = cx.state<std::size_t>("mode", 0);
        auto preview = cx.state<bool>("preview", true);
        auto recents = cx.state<bool>("recents", true);

        auto rows = entries();
        auto selected_index = selected.get() < rows.size()
            ? selected.get()
            : std::size_t{0};
        auto selected_entry = rows[selected_index];

        return ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
            ui::Text("File Explorer").font(ui::Font::hero_title),
            ui::Text("Desktop layout built from callback-driven components")
                .font(ui::Font::hero_subtitle)
                .color(phenotype::TextColor::Muted),
            ui::Card(
                ui::VStack(
                    ui::HStack(
                        ui::TextField("Search files", query.binding())
                            .semantic_label("File search")
                            .width(320.0f),
                        ui::Button("New Folder")
                            .role(ui::ButtonRole::primary)
                            .on_click([query] {
                                query.set("New Folder");
                            }),
                        ui::Button(preview.get()
                            ? "Hide Preview"
                            : "Show Preview")
                            .on_click([preview] {
                                preview.mutate([](bool& value) {
                                    value = !value;
                                });
                            })),
                    ui::View{[mode, recents] {
                        std::vector<phenotype::str> modes;
                        modes.emplace_back("Icons", 5u);
                        modes.emplace_back("List", 4u);
                        modes.emplace_back("Columns", 7u);
                        phenotype::widget::tabs(
                            modes,
                            mode.get(),
                            [mode](std::size_t index) {
                                mode.set(index);
                            },
                            phenotype::layout::glass_regular());
                        phenotype::layout::spacer(8);
                        phenotype::widget::glass_switch(
                            "Recents",
                            recents.get(),
                            [recents] {
                                recents.mutate([](bool& value) {
                                    value = !value;
                                });
                            });
                    }}))
                .padding(phenotype::SpaceToken::Lg)
                .glass(),
            ui::HStack(
                ui::StackOptions{
                    .spacing = phenotype::SpaceToken::Lg,
                    .cross = ui::AxisAlignment::start,
                },
                ui::Weighted(
                    0.42f,
                    ui::Card(ui::View{[recents, selected] {
                        phenotype::layout::column([&] {
                            phenotype::widget::text("Locations");
                            phenotype::layout::spacer(8);
                            phenotype::widget::glass_selection_button(
                                "Recents",
                                [recents] { recents.set(true); },
                                phenotype::GlassSelectionStyleOptions{
                                    .chrome =
                                        phenotype::GlassSelectionChrome::SidebarPill,
                                    .role =
                                        phenotype::MaterialSurfaceRole::Sidebar,
                                    .selected = recents.get(),
                                    .width = 190.0f,
                                });
                            phenotype::widget::glass_selection_button(
                                "Documents",
                                [recents, selected] {
                                    recents.set(false);
                                    selected.set(0);
                                },
                                phenotype::GlassSelectionStyleOptions{
                                    .chrome =
                                        phenotype::GlassSelectionChrome::SidebarPill,
                                    .role =
                                        phenotype::MaterialSurfaceRole::Sidebar,
                                    .selected = !recents.get(),
                                    .width = 190.0f,
                                });
                        });
                    }}).padding(phenotype::SpaceToken::Lg)),
                ui::Weighted(
                    1.0f,
                    ui::Card(ui::View{[
                        rows = std::move(rows),
                        selected,
                        query_value = query.get()] {
                        phenotype::layout::column([&] {
                            phenotype::widget::text(
                                query_value.empty()
                                    ? "Items"
                                    : std::string("Items matching ")
                                        + query_value);
                            phenotype::layout::spacer(8);
                            for (std::size_t i = 0; i < rows.size(); ++i) {
                                auto const& entry = rows[i];
                                phenotype::widget::glass_outline_row_button(
                                    label(entry.name + " - " + entry.kind),
                                    [selected, i] { selected.set(i); },
                                    phenotype::GlassOutlineRowStyleOptions{
                                        .chrome =
                                            phenotype::GlassOutlineRowChrome::ColumnRow,
                                        .role =
                                            phenotype::MaterialSurfaceRole::Content,
                                        .selected = selected.get() == i,
                                        .width = 420.0f,
                                        .height = 32.0f,
                                    });
                            }
                        });
                    }}).padding(phenotype::SpaceToken::Lg)),
                ui::Weighted(
                    0.55f,
                    ui::Card(ui::VStack(
                        ui::Text(preview.get() ? "Preview" : "Details")
                            .font(ui::Font::title),
                        ui::Text(selected_entry.name),
                        ui::Text(selected_entry.kind + " · "
                            + selected_entry.detail)
                            .color(phenotype::TextColor::Muted),
                        ui::Code(
                            "phenotype::widget::glass_outline_row_button(\n"
                            "    label, [selected] { selected.set(index); });"))))
                    .padding(phenotype::SpaceToken::Lg)
                    .glass()));
    }
};

int main() {
    return phenotype::native::run_app<FileExplorerDesktopApp>(
        1040,
        720,
        "Phenotype File Explorer");
}

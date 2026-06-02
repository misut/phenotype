#include <string>
#include <utility>
#include <vector>

import phenotype;
import phenotype.native;

namespace ui = phenotype::ui;

struct MobileEntry {
    unsigned int id = 0;
    std::string name;
    std::string summary;
};

static phenotype::str label(std::string_view value) {
    return phenotype::str{
        value.data(),
        static_cast<unsigned int>(value.size())};
}

static std::vector<MobileEntry> mobile_entries() {
    return {
        {1, "Inbox", "8 files"},
        {2, "Drafts", "3 documents"},
        {3, "Photos", "18 images"},
        {4, "Archive", "2 folders"},
    };
}

struct FileExplorerMobileApp {
    ui::View body(ui::Context& cx) const {
        auto tab = cx.state<std::size_t>("tab", 0);
        auto selected = cx.state<std::size_t>("selected", 0);
        auto search = cx.state<std::string>("search", {});
        auto draft_name = cx.state<std::string>("draft-name", "Notes.txt");
        auto rows = mobile_entries();
        auto active = rows[selected.get() < rows.size() ? selected.get() : 0];

        return ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
            ui::Text("Mobile Files").font(ui::Font::hero_title),
            ui::Card(
                ui::VStack(
                    ui::View{[tab] {
                        std::vector<phenotype::str> tabs;
                        tabs.emplace_back("Browse", 6u);
                        tabs.emplace_back("Preview", 7u);
                        tabs.emplace_back("Create", 6u);
                        phenotype::widget::tabs(
                            tabs,
                            tab.get(),
                            [tab](std::size_t index) { tab.set(index); },
                            phenotype::layout::glass_regular());
                    }},
                    ui::TextField("Search", search.binding())
                        .semantic_label("Mobile file search"),
                    ui::View{[rows = std::move(rows), selected] {
                        phenotype::layout::column([&] {
                            for (std::size_t i = 0; i < rows.size(); ++i) {
                                auto const& row = rows[i];
                                phenotype::widget::glass_menu_item_button(
                                    label(row.name + " · " + row.summary),
                                    [selected, i] { selected.set(i); },
                                    phenotype::GlassMenuItemStyleOptions{
                                        .width = 320.0f,
                                        .height = 34.0f,
                                    });
                            }
                        });
                    }},
                    ui::Text("Selected: " + active.name)
                        .color(phenotype::TextColor::Muted),
                    ui::TextField("New file name", draft_name.binding())
                        .semantic_label("New file name"),
                    ui::HStack(
                        ui::Button("Duplicate").on_click([draft_name] {
                            draft_name.set("Copy of " + draft_name.get());
                        }),
                        ui::Button("Create")
                            .role(ui::ButtonRole::primary)
                            .on_click([tab] { tab.set(2); }))))
                .padding(phenotype::SpaceToken::Lg)
                .frame({.max_width = 390})
                .glass(),
            ui::Card(
                ui::VStack(
                    ui::Text("Callback Pattern").font(ui::Font::title),
                    ui::Code(
                        "ui::TextField(\"Search\", search.binding());\n"
                        "widget::tabs(items, tab.get(), [tab](std::size_t i) {\n"
                        "    tab.set(i);\n"
                        "});")))
                .padding(phenotype::SpaceToken::Lg)
                .frame({.max_width = 390}));
    }
};

int main() {
    return phenotype::native::run_app<FileExplorerMobileApp>(
        430,
        720,
        "Phenotype Mobile Files");
}

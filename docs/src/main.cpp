#include <string>
#include <utility>
#include <vector>

import phenotype;

namespace ui = phenotype::ui;

struct Sample {
    std::string title;
    std::string code;
};

struct DocsApp {
    ui::View body(ui::Context& cx) const {
        auto count = cx.state<int>("count", 0);
        auto input = cx.state<std::string>("input", {});
        auto accepted = cx.state<bool>("accepted", false);
        auto plan = cx.state<std::size_t>("plan", 1);

        auto samples = std::vector<Sample>{
            Sample{
                .title = "Component state",
                .code =
                    "auto count = cx.state<int>(\"count\", 0);\n"
                    "ui::Button(\"+\").on_click([count] {\n"
                    "    count.mutate([](int& value) { ++value; });\n"
                    "});",
            },
            Sample{
                .title = "Text binding",
                .code =
                    "auto name = cx.state<std::string>(\"name\", {});\n"
                    "ui::TextField(\"Name\", name.binding())\n"
                    "    .semantic_label(\"Profile name\");",
            },
            Sample{
                .title = "Low-level callbacks",
                .code =
                    "widget::checkbox(\"Accept\", accepted.get(), [accepted] {\n"
                    "    accepted.mutate([](bool& value) { value = !value; });\n"
                    "});",
            },
        };

        auto options = std::vector<phenotype::str>{};
        options.emplace_back("Free", 4u);
        options.emplace_back("Pro", 3u);
        options.emplace_back("Team", 4u);

        return ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
            ui::Text("Phenotype").font(ui::Font::hero_title),
            ui::Text("Modern C++23 declarative UI")
                .font(ui::Font::hero_subtitle)
                .color(phenotype::TextColor::Muted),
            ui::Card(
                ui::VStack(
                    ui::Text("Interactive Sample").font(ui::Font::title),
                    ui::Text("count=" + std::to_string(count.get()))
                        .color(phenotype::TextColor::Muted),
                    ui::HStack(
                        ui::Button("-").on_click([count] {
                            count.mutate([](int& value) { --value; });
                        }),
                        ui::Button("+")
                            .role(ui::ButtonRole::primary)
                            .on_click([count] {
                                count.mutate([](int& value) { ++value; });
                            })),
                    ui::TextField("Type something", input.binding())
                        .semantic_label("Documentation input"),
                    ui::Text("input=" + input.get())
                        .color(phenotype::TextColor::Muted),
                    ui::View{[accepted, plan, options = std::move(options)] {
                        phenotype::layout::column([&] {
                            phenotype::widget::checkbox(
                                "Accept terms",
                                accepted.get(),
                                [accepted] {
                                    accepted.mutate([](bool& value) {
                                        value = !value;
                                    });
                                });
                            phenotype::layout::spacer(8);
                            phenotype::widget::tabs(
                                options,
                                plan.get(),
                                [plan](std::size_t index) {
                                    plan.set(index);
                                });
                        });
                    }}))
                .padding(phenotype::SpaceToken::Lg)
                .frame({.max_width = 560})
                .glass(),
            ui::Card(
                ui::VStack(
                    ui::Text("API Notes").font(ui::Font::title),
                    ui::ForEach<Sample>(
                        std::move(samples),
                        [](Sample const& item) {
                            return ui::stable_id(item.title);
                        },
                        [](Sample const& item) {
                            return ui::VStack(
                                ui::Text(item.title).font(ui::Font::caption),
                                ui::Code(item.code));
                        })))
                .padding(phenotype::SpaceToken::Lg)
                .frame({.max_width = 560}));
    }
};

int main() {
    ui::run<DocsApp>();
    return 0;
}

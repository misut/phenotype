#include <string>
#include <utility>
#include <vector>

import phenotype;
import phenotype.native;

namespace ui = phenotype::ui;

struct Activity {
    unsigned int id = 0;
    std::string label;
};

struct NativeShowcaseApp {
    ui::View body(ui::Context& cx) const {
        auto count = cx.state<int>("count", 0);
        auto next_id = cx.state<unsigned int>("next-id", 1);
        auto name = cx.state<std::string>("name", {});
        auto ime = cx.state<std::string>("ime", {});
        auto validation = cx.state<std::string>("validation", "Too short");
        auto notifications = cx.state<bool>("notifications", true);
        auto inspector = cx.state<bool>("inspector", true);
        auto plan = cx.state<std::size_t>("plan", 1);
        auto rows = cx.state<std::vector<Activity>>("activity", {});

        auto remember = [rows, next_id](std::string label) {
            auto id = next_id.get();
            next_id.set(id + 1);
            rows.mutate([&](std::vector<Activity>& items) {
                items.push_back(Activity{.id = id, .label = std::move(label)});
                if (items.size() > 7)
                    items.erase(items.begin());
            });
        };

        auto plan_items = std::vector<phenotype::str>{};
        plan_items.emplace_back("Free", 4u);
        plan_items.emplace_back("Pro", 3u);
        plan_items.emplace_back("Team", 4u);

        auto activity = rows.get();

        auto counter_card = ui::Card(ui::VStack(
            ui::Text("Counter").font(ui::Font::title),
            ui::Text("value=" + std::to_string(count.get()))
                .color(phenotype::TextColor::Muted),
            ui::HStack(
                ui::Button("-").on_click([count, remember] {
                    count.mutate([](int& value) { --value; });
                    remember("decrement");
                }),
                ui::Button("+")
                    .role(ui::ButtonRole::primary)
                    .on_click([count, remember] {
                        count.mutate([](int& value) { ++value; });
                        remember("increment");
                    })),
            ui::Code(
                "ui::Button(\"+\").on_click([count] {\n"
                "    count.mutate([](int& value) { ++value; });\n"
                "});")))
            .padding(phenotype::SpaceToken::Lg)
            .glass();

        auto input_card = ui::Card(ui::VStack(
            ui::Text("Input").font(ui::Font::title),
            ui::TextField("Enter your name", name.binding())
                .semantic_label("Name"),
            ui::TextField("IME composition sample", ime.binding())
                .semantic_label("IME sample"),
            ui::TextField("Validation sample", validation.binding())
                .error(validation.get().size() < 4),
            ui::Text("Hello " + (name.get().empty()
                ? std::string{"friend"}
                : name.get()))
                .color(phenotype::TextColor::Muted)))
            .padding(phenotype::SpaceToken::Lg)
            .glass();

        auto controls_card = ui::Card(ui::View{[
            notifications,
            inspector,
            plan,
            remember,
            plan_items = std::move(plan_items)] {
            phenotype::layout::column([&] {
                phenotype::widget::text("Controls");
                phenotype::layout::spacer(8);
                phenotype::widget::glass_checkbox(
                    "Notifications",
                    notifications.get(),
                    [notifications, remember] {
                        notifications.mutate([](bool& value) {
                            value = !value;
                        });
                        remember("toggle notifications");
                    });
                phenotype::widget::glass_switch(
                    "Inspector",
                    inspector.get(),
                    [inspector, remember] {
                        inspector.mutate([](bool& value) {
                            value = !value;
                        });
                        remember("toggle inspector");
                    });
                phenotype::layout::spacer(8);
                phenotype::widget::tabs(
                    plan_items,
                    plan.get(),
                    [plan, remember](std::size_t index) {
                        plan.set(index);
                        remember("select plan");
                    },
                    phenotype::layout::glass_regular(),
                    phenotype::TabsStyleOptions{
                        .indicator_glass_identity =
                            phenotype::layout::glass_effect_identity(
                                "native",
                                "plan-indicator"),
                    });
                if (inspector.get()) {
                    phenotype::layout::spacer(8);
                    phenotype::widget::code(
                        "widget::tabs(items, selected, "
                        "[state](std::size_t index) { state.set(index); });");
                }
            });
        }})
            .padding(phenotype::SpaceToken::Lg)
            .glass();

        auto activity_card = ui::Card(ui::VStack(
            ui::Text("Activity").font(ui::Font::title),
            ui::ForEach<Activity>(
                std::move(activity),
                [](Activity const& item) { return item.id; },
                [](Activity const& item) {
                    return ui::Text(item.label);
                })))
            .padding(phenotype::SpaceToken::Lg)
            .frame({.max_width = 760});

        return ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Lg},
            ui::Text("Native Showcase").font(ui::Font::hero_title),
            ui::Text("Callbacks, bindings, tabs, text input, and glass controls")
                .font(ui::Font::hero_subtitle)
                .color(phenotype::TextColor::Muted),
            ui::HStack(
                ui::StackOptions{
                    .spacing = phenotype::SpaceToken::Lg,
                    .cross = ui::AxisAlignment::start,
                },
                ui::Weighted(1.0f, counter_card),
                ui::Weighted(1.0f, input_card)),
            controls_card,
            activity_card);
    }
};

int main() {
    return phenotype::native::run_app<NativeShowcaseApp>(
        920,
        720,
        "Phenotype Native Showcase");
}

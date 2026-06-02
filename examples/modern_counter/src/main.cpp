#include <cstdint>
#include <string>
#include <vector>

import phenotype;
import phenotype.native;
import std;

namespace ui = phenotype::ui;

struct HistoryItem {
    std::uint32_t id = 0;
    std::string label;
};

struct ModernCounterApp {
    ui::View body(ui::Context& cx) const {
        auto count = cx.state<int>("count", 0);
        auto note = cx.state<std::string>("note", {});
        auto next_id = cx.state<std::uint32_t>("next-history-id", 1);
        auto history = cx.state<std::vector<HistoryItem>>("history", {});

        auto append_history = [history, next_id](std::string label) {
            auto id = next_id.get();
            next_id.set(id + 1);
            history.update([&](std::vector<HistoryItem>& rows) {
                rows.push_back(HistoryItem{
                    .id = id,
                    .label = std::move(label),
                });
                if (rows.size() > 6)
                    rows.erase(rows.begin());
            });
        };

        auto rows = history.get();

        return ui::VStack(
            ui::StackOptions{.spacing = phenotype::SpaceToken::Md},
            ui::Text("Modern Counter").font(ui::Font::title),
            ui::Text("count=" + std::to_string(count.get()))
                .color(phenotype::TextColor::Muted),
            ui::HStack(
                ui::Button("-")
                    .on_click([count, append_history] {
                        count.update([](int& value) { --value; });
                        append_history("decrement");
                    }),
                ui::Button("+")
                    .role(ui::ButtonRole::primary)
                    .on_click([count, append_history] {
                        count.update([](int& value) { ++value; });
                        append_history("increment");
                    })),
            ui::TextField("Add a note", note.binding())
                .semantic_label("Counter note"),
            ui::Text("note=" + note.get())
                .color(phenotype::TextColor::Muted),
            ui::Card(
                ui::VStack(
                    ui::Text("History").font(ui::Font::caption),
                    ui::ForEach<HistoryItem>(
                        std::move(rows),
                        [](HistoryItem const& item) { return item.id; },
                        [](HistoryItem const& item) {
                            return ui::Text(item.label);
                        }))))
            .padding(phenotype::SpaceToken::Lg)
            .frame({.max_width = 420})
            .glass();
    }
};

int main() {
    return phenotype::native::run_app<ModernCounterApp>(
        420,
        520,
        "Modern Counter");
}

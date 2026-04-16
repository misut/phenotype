#include <string>
#include <variant>

import phenotype;
import phenotype.native;

// ---- Messages ----

struct Increment {};
struct Decrement {};
struct NameChanged { std::string text; };
struct ToggleAgreed {};
struct SetChoice { int value; };

using Msg = std::variant<Increment, Decrement, NameChanged, ToggleAgreed, SetChoice>;

// ---- State ----

struct State {
    int count = 0;
    std::string name;
    bool agreed = false;
    int choice = 0;
};

// ---- Update ----

void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, Increment>)      state.count += 1;
        else if constexpr (std::same_as<T, Decrement>)  state.count -= 1;
        else if constexpr (std::same_as<T, NameChanged>) state.name = m.text;
        else if constexpr (std::same_as<T, ToggleAgreed>) state.agreed = !state.agreed;
        else if constexpr (std::same_as<T, SetChoice>)  state.choice = m.value;
    }, msg);
}

// ---- View ----

static Msg on_name_changed(std::string s) { return NameChanged{std::move(s)}; }

void view(State const& state) {
    using namespace phenotype;

    layout::column([&] {
        widget::text("phenotype — native widget showcase");
        layout::spacer(8);

        layout::card([&] {
            widget::text("Counter");
            layout::spacer(4);
            widget::text(std::string("Count: ") + std::to_string(state.count));
            layout::spacer(4);
            layout::row(
                [&] { widget::button<Msg>("-", Decrement{}); },
                [&] { widget::button<Msg>("+", Increment{}); }
            );
        });

        layout::spacer(8);

        layout::card([&] {
            widget::text("Text Input");
            layout::spacer(4);
            widget::text_field<Msg>("Enter your name...", state.name, on_name_changed);
            if (!state.name.empty()) {
                layout::spacer(4);
                widget::text(std::string("Hello, ") + state.name + "!");
            }
        });

        layout::spacer(8);

        layout::card([&] {
            widget::text("Selection");
            layout::spacer(4);
            widget::checkbox<Msg>("I agree to the terms", state.agreed, ToggleAgreed{});
            layout::spacer(4);
            widget::radio<Msg>("Option A", state.choice == 0, SetChoice{0});
            widget::radio<Msg>("Option B", state.choice == 1, SetChoice{1});
            widget::radio<Msg>("Option C", state.choice == 2, SetChoice{2});
        });

        layout::spacer(8);

        layout::card([&] {
            widget::text("Code");
            layout::spacer(4);
            widget::code("import phenotype;\nimport phenotype.native;");
        });

        layout::spacer(8);
        layout::divider();
        layout::spacer(8);

        widget::link("phenotype on GitHub", "https://github.com/misut/phenotype");
    });
}

// ---- Entry point ----

int main() {
    return phenotype::native::run_app<State, Msg>(480, 720, "phenotype", view, update);
}

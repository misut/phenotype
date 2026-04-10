#include <concepts>
#include <string>
#include <type_traits>
#include <variant>
import phenotype;

// Messages — every event the counter app emits.
struct Increment {};
struct Decrement {};
struct Reset {};
using Msg = std::variant<Increment, Decrement, Reset>;

// State — single source of truth, owned by the framework runner.
struct State {
    int count = 0;
};

// update — pure transformation, only place state mutates.
void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, Increment>) state.count += 1;
        else if constexpr (std::same_as<T, Decrement>) state.count -= 1;
        else if constexpr (std::same_as<T, Reset>)     state.count  = 0;
    }, msg);
}

// view — pure function from state to UI tree.
void view(State const& state) {
    using namespace phenotype;
    layout::column([&] {
        widget::text("Counter");
        widget::text(std::string("Count: ") + std::to_string(state.count));
        layout::row(
            [&] { widget::button<Msg>("-",     Decrement{}); },
            [&] { widget::button<Msg>("+",     Increment{}); },
            [&] { widget::button<Msg>("reset", Reset{}); }
        );
    });
}

int main() {
    phenotype::run<State, Msg>(view, update);
    return 0;
}

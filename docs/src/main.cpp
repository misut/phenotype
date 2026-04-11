#include <functional>
#include <string>
#include <utility>
#include <variant>
import phenotype;

namespace docs {

// ============================================================
// example(source, live) — code-on-left / live-on-right helper
// ============================================================
//
// Each call renders a row with the source-code literal on the left
// (in the existing widget::code styling) and the live phenotype
// rendering of the same code on the right (wrapped in a layout::card
// for visual separation). Both sides are wrapped in layout::sized_box
// at the same width so every example row has the same 50/50 split
// regardless of how long the code line is. The source string is
// manually kept in sync with the lambda body — small price for the
// readability of raw-string-literal code over a fragile
// stringification macro.
inline constexpr float EXAMPLE_PANE_WIDTH = 320.0f;

inline void example(phenotype::str source, std::function<void()> live) {
    using namespace phenotype;
    layout::row([&] {
        layout::sized_box(EXAMPLE_PANE_WIDTH, [&] {
            widget::code(source);
        });
        layout::sized_box(EXAMPLE_PANE_WIDTH, [&] {
            layout::card([&] {
                live();
            });
        });
    });
}

// ============================================================
// Messages — every user-facing event the docs site emits
// ============================================================

struct Increment {};
struct Decrement {};
struct InputChanged { std::string text; };
struct ToggleSubscribed {};
struct PickPlan { std::string id; };

using Msg = std::variant<Increment, Decrement, InputChanged,
                         ToggleSubscribed, PickPlan>;

// ============================================================
// State — single source of truth, owned by the framework runner
// ============================================================

struct State {
    int count = 0;
    std::string input;
    bool subscribed = false;
    std::string plan = "free";
};

// ============================================================
// update — pure transformation, only place state mutates
// ============================================================

void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, Increment>)         state.count += 1;
        else if constexpr (std::same_as<T, Decrement>)    state.count -= 1;
        else if constexpr (std::same_as<T, InputChanged>) state.input = m.text;
        else if constexpr (std::same_as<T, ToggleSubscribed>) state.subscribed = !state.subscribed;
        else if constexpr (std::same_as<T, PickPlan>)     state.plan = m.id;
    }, msg);
}

// ============================================================
// view — pure function, state -> UI tree
// ============================================================

void view(State const& state) {
    using namespace phenotype;
    layout::scaffold(
        // Hero
        [&] {
            widget::text("phenotype");
            widget::text("A cross-platform C++ UI framework with WebGPU renderer");
        },
        // Content
        [&] {
            // Features
            layout::column([&] {
                widget::text("Features");
                layout::list_items([&] {
                    layout::item("WebGPU 2D renderer (WGSL shaders, instanced draws, text atlas)");
                    layout::item("Compose-style declarative DSL (column, row, scaffold, card)");
                    layout::item("Flexbox layout engine with alignment (main/cross axis)");
                    layout::item("Pure-function view with typed messages (Iced/Elm pattern)");
                    layout::item("Hover states and keyboard navigation (Tab/Enter)");
                    layout::item("Text input (text_field) with caret and placeholder");
                    layout::item("Theme system with design tokens");
                    layout::item("Scroll and resize support with viewport culling");
                });
            });

            // TextField demo
            layout::column([&] {
                widget::text("Text Input");
                widget::text("Click the field and type:");

                layout::column([&] {
                    widget::text_field<Msg>("Type something...", state.input,
                        +[](std::string s) -> Msg { return InputChanged{std::move(s)}; });
                    widget::text(std::string("Echo: ") + state.input);
                });
            });

            layout::divider();

            // Quick start
            layout::column([&] {
                widget::text("Quick Start");
                widget::code(
                    "# Install tools\n"
                    "curl -fsSL https://raw.githubusercontent.com/misut/exon/main/install.sh | sh\n"
                    "curl -fsSL https://raw.githubusercontent.com/misut/intron/main/install.sh | sh\n"
                    "\n"
                    "# Create project\n"
                    "exon init --lib my-app\n"
                    "exon add github.com/misut/phenotype 0.10.0\n"
                    "\n"
                    "# Build for WebAssembly\n"
                    "exon build --target wasm32-wasi"
                );
            });

            // Examples — each widget / layout primitive shown alongside
            // its live rendering. The example() helper renders the source
            // literal on the left (widget::code) and runs the lambda on
            // the right (inside a layout::card).
            layout::column([&] {
                widget::text("Examples");
                widget::text("Each widget and layout below is shown next to its live "
                             "rendering so you can see exactly what the code produces.");

                example(
                    R"(widget::text("Hello, phenotype");)",
                    [&] {
                        widget::text("Hello, phenotype");
                    });

                example(
                    R"(widget::link("GitHub", "https://github.com/misut/phenotype");)",
                    [&] {
                        widget::link("GitHub", "https://github.com/misut/phenotype");
                    });

                example(
                    R"(widget::code("exon build --target wasm32-wasi");)",
                    [&] {
                        widget::code("exon build --target wasm32-wasi");
                    });

                example(
                    R"(widget::button<Msg>("Click me", Increment{});)",
                    [&] {
                        widget::button<Msg>("Click me", Increment{});
                    });

                example(
                    R"(widget::text_field<Msg>(
    "Type here...",
    state.input,
    +[](std::string s) -> Msg { return InputChanged{std::move(s)}; });)",
                    [&] {
                        widget::text_field<Msg>(
                            "Type here...",
                            state.input,
                            +[](std::string s) -> Msg { return InputChanged{std::move(s)}; });
                    });

                example(
                    R"(widget::checkbox<Msg>(
    "Subscribe to updates",
    state.subscribed,
    ToggleSubscribed{});)",
                    [&] {
                        widget::checkbox<Msg>(
                            "Subscribe to updates",
                            state.subscribed,
                            ToggleSubscribed{});
                    });

                example(
                    R"(widget::radio<Msg>("Free", state.plan == "free", PickPlan{"free"});
widget::radio<Msg>("Pro",  state.plan == "pro",  PickPlan{"pro"});
widget::radio<Msg>("Team", state.plan == "team", PickPlan{"team"});)",
                    [&] {
                        layout::column([&] {
                            widget::radio<Msg>("Free", state.plan == "free", PickPlan{"free"});
                            widget::radio<Msg>("Pro",  state.plan == "pro",  PickPlan{"pro"});
                            widget::radio<Msg>("Team", state.plan == "team", PickPlan{"team"});
                        });
                    });

                example(
                    R"(layout::column([&] {
    widget::text("First");
    widget::text("Second");
    widget::text("Third");
});)",
                    [&] {
                        layout::column([&] {
                            widget::text("First");
                            widget::text("Second");
                            widget::text("Third");
                        });
                    });

                example(
                    R"(layout::row([&] {
    widget::text("Left");
    widget::text("Middle");
    widget::text("Right");
});)",
                    [&] {
                        layout::row([&] {
                            widget::text("Left");
                            widget::text("Middle");
                            widget::text("Right");
                        });
                    });

                example(
                    R"(layout::card([&] {
    widget::text("Inside a card");
});)",
                    [&] {
                        layout::card([&] {
                            widget::text("Inside a card");
                        });
                    });

                example(
                    R"(layout::list_items([&] {
    layout::item("First item");
    layout::item("Second item");
    layout::item("Third item");
});)",
                    [&] {
                        layout::list_items([&] {
                            layout::item("First item");
                            layout::item("Second item");
                            layout::item("Third item");
                        });
                    });

                example(
                    R"(layout::divider();)",
                    [&] {
                        layout::divider();
                    });

                example(
                    R"(layout::spacer(24);)",
                    [&] {
                        layout::spacer(24);
                    });
            });
        },
        // Footer
        [&] {
            layout::row(
                [&] { widget::link("GitHub", "https://github.com/misut/phenotype"); },
                [&] { widget::text(" · "); },
                [&] { widget::link("MIT License", "https://github.com/misut/phenotype/blob/main/LICENSE"); }
            );
        }
    );
}

} // namespace docs

int main() {
    phenotype::run<docs::State, docs::Msg>(docs::view, docs::update);
    return 0;
}

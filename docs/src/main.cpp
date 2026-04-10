#include <string>
#include <utility>
#include <variant>
import phenotype;

namespace docs {

// ============================================================
// Messages — every user-facing event the docs site emits
// ============================================================

struct Increment {};
struct Decrement {};
struct InputChanged { std::string text; };

using Msg = std::variant<Increment, Decrement, InputChanged>;

// ============================================================
// State — single source of truth, owned by the framework runner
// ============================================================

struct State {
    int count = 0;
    std::string input;
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

            // Interactive demo — counter
            layout::column([&] {
                widget::text("Interactive Demo");
                widget::text("Click the buttons or use Tab + Enter:");

                layout::column([&] {
                    widget::text(std::string("Count: ") + std::to_string(state.count));
                    layout::row(
                        [&] { widget::button<Msg>("-", Decrement{}); },
                        [&] { widget::button<Msg>("+", Increment{}); }
                    );
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
                    "exon add github.com/misut/phenotype 0.7.0\n"
                    "\n"
                    "# Build for WebAssembly\n"
                    "exon build --target wasm32-wasi"
                );
            });

            // Component reference
            layout::column([&] {
                widget::text("Components");
                layout::row([&] { widget::code("layout::column(builder)");          widget::text("Vertical flex layout"); });
                layout::row([&] { widget::code("layout::row(builder)");             widget::text("Horizontal flex layout"); });
                layout::row([&] { widget::code("widget::text(str)");                widget::text("Text display"); });
                layout::row([&] { widget::code("widget::button<Msg>(label, msg)");  widget::text("Clickable button posting a message"); });
                layout::row([&] { widget::code("widget::text_field<Msg>(...)");     widget::text("Text input mapping value to message"); });
                layout::row([&] { widget::code("widget::link(label, url)");         widget::text("Hyperlink"); });
                layout::row([&] { widget::code("widget::code(str)");                widget::text("Monospace code block"); });
                layout::row([&] { widget::code("layout::scaffold(top, body, bot)"); widget::text("Page layout with hero"); });
                layout::row([&] { widget::code("layout::card(builder)");            widget::text("Rounded container"); });
                layout::row([&] { widget::code("layout::list_items(builder)");      widget::text("Bullet list"); });
                layout::row([&] { widget::code("layout::divider()");                widget::text("Horizontal separator"); });
                layout::row([&] { widget::code("layout::spacer(px)");               widget::text("Vertical spacing"); });
                layout::row([&] { widget::code("run<State, Msg>(view, update)");    widget::text("Application entry point"); });
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

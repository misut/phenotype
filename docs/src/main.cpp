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
    Scaffold(
        // Hero
        [&] {
            Text("phenotype");
            Text("A cross-platform C++ UI framework with WebGPU renderer");
        },
        // Content
        [&] {
            // Features
            Column([&] {
                Text("Features");
                ListItems([&] {
                    Item("WebGPU 2D renderer (WGSL shaders, instanced draws, text atlas)");
                    Item("Compose-style declarative DSL (Column, Row, Scaffold, Card)");
                    Item("Flexbox layout engine with alignment (main/cross axis)");
                    Item("Pure-function view with typed messages (Iced/Elm pattern)");
                    Item("Hover states and keyboard navigation (Tab/Enter)");
                    Item("Text input (TextField) with caret and placeholder");
                    Item("Theme system with design tokens");
                    Item("Scroll and resize support with viewport culling");
                });
            });

            // Interactive demo — counter
            Column([&] {
                Text("Interactive Demo");
                Text("Click the buttons or use Tab + Enter:");

                Column([&] {
                    Text(std::string("Count: ") + std::to_string(state.count));
                    Row(
                        [&] { Button<Msg>("-", Decrement{}); },
                        [&] { Button<Msg>("+", Increment{}); }
                    );
                });
            });

            // TextField demo
            Column([&] {
                Text("Text Input");
                Text("Click the field and type:");

                Column([&] {
                    TextField<Msg>("Type something...", state.input,
                        +[](std::string s) -> Msg { return InputChanged{std::move(s)}; });
                    Text(std::string("Echo: ") + state.input);
                });
            });

            Divider();

            // Quick start
            Column([&] {
                Text("Quick Start");
                Code(
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
            Column([&] {
                Text("Components");
                Row([&] { Code("Column(builder)");                 Text("Vertical flex layout"); });
                Row([&] { Code("Row(builder)");                    Text("Horizontal flex layout"); });
                Row([&] { Code("Text(str)");                       Text("Text display"); });
                Row([&] { Code("Button<Msg>(label, msg)");         Text("Clickable button posting a message"); });
                Row([&] { Code("TextField<Msg>(hint, val, fn)");   Text("Text input mapping value to message"); });
                Row([&] { Code("Link(label, url)");                Text("Hyperlink"); });
                Row([&] { Code("Code(str)");                       Text("Monospace code block"); });
                Row([&] { Code("Scaffold(top, body, bot)");        Text("Page layout with hero"); });
                Row([&] { Code("Card(builder)");                   Text("Rounded container"); });
                Row([&] { Code("ListItems(builder)");              Text("Bullet list"); });
                Row([&] { Code("Divider()");                       Text("Horizontal separator"); });
                Row([&] { Code("Spacer(px)");                      Text("Vertical spacing"); });
                Row([&] { Code("run<State, Msg>(view, update)");   Text("Application entry point"); });
            });
        },
        // Footer
        [&] {
            Row(
                [&] { Link("GitHub", "https://github.com/misut/phenotype"); },
                [&] { Text(" · "); },
                [&] { Link("MIT License", "https://github.com/misut/phenotype/blob/main/LICENSE"); }
            );
        }
    );
}

} // namespace docs

int main() {
    phenotype::run<docs::State, docs::Msg>(docs::view, docs::update);
    return 0;
}

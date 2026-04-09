#include <string>
import phenotype;
using namespace phenotype;

void DocsApp() {
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
                    Item("Reactive state (Trait<T>) with partial mutation");
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

                auto count = encode(0);
                Column([count] {
                    Text(std::string("Count: ") + std::to_string(count->value()));
                    Row(
                        [count] { Button("-", [count] { count->set(count->value() - 1); }); },
                        [count] { Button("+", [count] { count->set(count->value() + 1); }); }
                    );
                });
            });

            // TextField demo
            Column([&] {
                Text("Text Input");
                Text("Click the field and type:");

                auto input = encode(std::string{});
                Column([input] {
                    TextField(input, "Type something...");
                    Text(std::string("Echo: ") + input->value());
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
                    "exon add github.com/misut/phenotype 0.6.0\n"
                    "\n"
                    "# Build for WebAssembly\n"
                    "exon build --target wasm32-wasi"
                );
            });

            // Component reference
            Column([&] {
                Text("Components");
                Row([&] { Code("Column(builder)");          Text("Vertical flex layout"); });
                Row([&] { Code("Row(builder)");             Text("Horizontal flex layout"); });
                Row([&] { Code("Text(str)");                Text("Text display"); });
                Row([&] { Code("Button(label, callback)");  Text("Clickable button with hover"); });
                Row([&] { Code("TextField(state, hint)");   Text("Text input with caret"); });
                Row([&] { Code("Link(label, url)");         Text("Hyperlink"); });
                Row([&] { Code("Code(str)");                Text("Monospace code block"); });
                Row([&] { Code("Scaffold(top, body, bot)"); Text("Page layout with hero"); });
                Row([&] { Code("Card(builder)");            Text("Rounded container"); });
                Row([&] { Code("ListItems(builder)");       Text("Bullet list"); });
                Row([&] { Code("Divider()");                Text("Horizontal separator"); });
                Row([&] { Code("Spacer(px)");               Text("Vertical spacing"); });
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

int main() {
    express(DocsApp);
    return 0;
}

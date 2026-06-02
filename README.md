# phenotype

`phenotype` is a compact C++23 declarative UI framework for WASI and small
native prototypes. Applications are plain component objects whose
`body(ui::Context&)` returns a `ui::View`. Local UI state is typed, callbacks
are ordinary C++ callables, and rendering targets a small browser-hosted canvas
command contract.

```cpp
import phenotype.native;

struct CounterApp {
    phenotype::ui::View body(phenotype::ui::Context& cx) {
        namespace ui = phenotype::ui;

        auto count = cx.state<int>("count", 0);

        return ui::VStack(
            ui::Text("Counter").font(ui::Font::title),
            ui::Text("count=" + std::to_string(count.get())),
            ui::Button("+")
                .role(ui::ButtonRole::primary)
                .on_click([count] {
                    count.set(count.get() + 1);
                }));
    }
};

int main() {
    return phenotype::native::run_app(480, 320, "Counter", CounterApp{});
}
```

## Public Surface

| API | Purpose |
| --- | --- |
| `ui::View` | Type-erased renderable component fragment |
| `ui::Context` | Viewport helpers and keyed typed local state |
| `ui::State<T>` | State holder with `get`, `set`, `mutate`, and `binding` |
| `ui::Binding<T>` | Two-way value binding for text controls |
| `ui::Text`, `ui::Button`, `ui::TextField` | Core interactive components |
| `ui::Checkbox`, `ui::Radio`, `ui::Switch`, `ui::Tabs` | Selection controls |
| `ui::VStack`, `ui::HStack`, `ui::Weighted`, `ui::Card` | Layout composition |
| `ui::run<App>()` | WASI/browser entry point |
| `native::run_app(..., App)` | Lightweight native prototype entry point |

The API follows a small set of design rules:

- Prefer clarity at the use site over broad compatibility coverage.
- Keep state close to the component that owns it, and pass bindings only where
  a child needs to edit the value.
- Derive display values during rendering instead of storing duplicate state.
- Keep identity explicit with keys and stable state names.
- Keep C++ ownership ordinary: values, references, and lambdas decide lifetime.

## Build

```sh
mise install
mise exec -- intron install
mise exec -- exon test
```

Build the documentation site:

```sh
cd docs
mise exec -- exon build --target wasm32-wasi
```

## WASI Preview CLI

The `cli` package builds the documentation WASI binary, stages the browser
shim, and serves the result with the `cppx` HTTP server.

```sh
cd cli
mise exec -- exon build
cd ..
cli/.exon/debug/phenotype-cli test
cli/.exon/debug/phenotype-cli serve --open
```

Useful commands:

| Command | Purpose |
| --- | --- |
| `phenotype-cli build` | Build `docs` for `wasm32-wasi` and stage `index.html`, `phenotype.js`, and `docs.wasm` |
| `phenotype-cli serve` | Build, stage, and serve the browser site at `http://127.0.0.1:4174/` |
| `phenotype-cli test` | Build, stage, start a temporary HTTP server, and verify the HTML, JS, and WASM responses |

Pass `--no-build` to reuse an existing `docs` artifact, `--release` to stage a
release build, and `--out-dir <dir>` to choose the preview directory.

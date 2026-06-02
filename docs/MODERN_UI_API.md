# Modern UI API

The default phenotype interface is moving from an Elm-style
`State` / `Msg` / `update` loop to a modern C++23 declarative component API.
The legacy runner remains available for compatibility, but new applications
should prefer `phenotype::ui`.

## Reference Model

The API intentionally combines ideas from several mature UI systems:

- SwiftUI: value-like view descriptions, custom view structs, `body`, bindings,
  environment-style contextual reads, and fluent modifiers.
- Jetpack Compose: explicit state ownership, state hoisting, slot APIs, and
  ordered immutable modifier chains.
- React: render snapshots and explicit keys for preserving or resetting local
  state under conditional rendering and lists.
- Solid and Qwik: signal-like local state handles that update only the UI that
  reads them, without requiring a global reducer.
- Dear ImGui: minimal duplicated UI state from the user's point of view, while
  still allowing phenotype to keep retained runtime state internally.

The C++ surface differs from those frameworks where the language demands it:

- Components are ordinary structs with `body(ui::Context&)`.
- State handles are RAII-friendly non-owning typed handles returned from
  `Context::state<T>()`.
- Text fields and child components share state through `ui::Binding<T>`.
- Callbacks are typed C++ callables instead of message variants.
- Options use aggregate structs and designated initializers when named
  parameters make code easier to read.
- Identity is explicit with `View::key(...)` and `ui::ForEach(...)`.

Primary references:

- SwiftUI custom views and data flow:
  <https://developer.apple.com/documentation/swiftui/declaring-a-custom-view>
- Jetpack Compose modifiers:
  <https://developer.android.com/develop/ui/compose/modifiers>
- Jetpack Compose state hoisting:
  <https://developer.android.com/jetpack/compose/state-hoisting>
- React state snapshots and keyed identity:
  <https://react.dev/learn/state-as-a-snapshot>
- React preserving and resetting state:
  <https://react.dev/learn/preserving-and-resetting-state>
- Solid fine-grained reactivity:
  <https://docs.solidjs.com/advanced-concepts/fine-grained-reactivity>

## Minimal App

```cpp
import phenotype;
import std;

namespace ui = phenotype::ui;

struct CounterApp {
    ui::View body(ui::Context& cx) const {
        auto count = cx.state<int>("count", 0);

        return ui::VStack(
            ui::Text("Counter").font(ui::Font::title),
            ui::Text("count=" + std::to_string(count.get())),
            ui::Button("+")
                .role(ui::ButtonRole::primary)
                .on_click([count] {
                    count.update([](int& value) { ++value; });
                }));
    }
};

int main() {
    phenotype::ui::run<CounterApp>();
}
```

Native test and example hosts can pass their host explicitly:

```cpp
phenotype::null_host host;
phenotype::ui::run<CounterApp>(host);
```

## State And Binding

State belongs to the closest component that owns the UI logic. Simple UI state
can stay local:

```cpp
auto expanded = cx.state<bool>("expanded", false);
```

Shared or externally controlled values should be passed down as bindings:

```cpp
struct SearchBox {
    ui::Binding<std::string> text;

    ui::View body(ui::Context&) const {
        return ui::TextField("Search", text)
            .semantic_label("Search files");
    }
};
```

## Lists And Identity

Use stable keys for data-driven collections. Keys are local to the parent, just
like React keys and Compose item keys.

```cpp
auto rows = std::vector<File>{/* ... */};

return ui::ForEach<File>(
    std::move(rows),
    [](File const& file) { return file.id; },
    [](File const& file) {
        return ui::Text(file.name);
    });
```

Use `View::key(...)` for conditional branches where state must reset when the
logical entity changes.

## Modifier Guidance

Modifiers wrap a `ui::View` and are ordered. The call order must remain
meaningful and visible at the call site:

```cpp
return ui::VStack(...)
    .padding(phenotype::SpaceToken::Lg)
    .frame({ .max_width = 420 })
    .glass();
```

Prefer component parameters for semantic inputs and modifiers for layout,
appearance, interaction, and accessibility metadata. If a modifier starts
needing many fields, add an aggregate options type instead of a long positional
parameter list.

## Compatibility

The legacy `phenotype::run<State, Msg>(view, update)` entry point remains in
place for older examples and downstream apps. It should be treated as a
compatibility layer; new docs and examples should use `phenotype::ui`.

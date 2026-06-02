# phenotype

`phenotype` is a standalone C++23 declarative UI framework for native and
WASI targets. Applications are written as small component objects whose
`body(ui::Context&)` returns a `ui::View`; state lives in typed state holders,
bindings, ordinary members, or captured domain objects.

```cpp
import phenotype.native;

struct CounterApp {
    phenotype::ui::View body(phenotype::ui::Context& ctx) {
        using namespace phenotype;
        namespace ui = phenotype::ui;

        auto count = ctx.state<int>("count", 0);

        return ui::VStack(
            ui::Text("Counter").font(ui::Font::title),
            ui::Text(std::format("{}", count.get())),
            ui::HStack(
                ui::Button("-").on_click([count] {
                    count.mutate([](int& value) { --value; });
                }),
                ui::Button("+").role(ui::ButtonRole::primary).on_click([count] {
                    count.mutate([](int& value) { ++value; });
                })));
    }
};

int main() {
    return phenotype::native::run_app(480, 320, "Counter", CounterApp{});
}
```

## Design Shape

The public API follows a component-first model:

| API | Purpose |
| --- | --- |
| `ui::View` | Type-erased renderable component fragment |
| `ui::Context` | Per-render access to call-site stable local state |
| `ui::State<T>` | Typed state holder with `get`, `set`, `mutate`, and `binding` |
| `ui::Binding<T>` | Two-way value binding for input controls |
| `ui::Text`, `ui::Button`, `ui::TextField` | Component wrappers for common widgets |
| `ui::VStack`, `ui::HStack`, `ui::Box`, `ui::ForEach` | Layout composition helpers |
| `ui::run<App>()`, `ui::run<App>(host)` | Component runner for WASI/native hosts |
| `native::run_app(..., App)` | Desktop native entry point |

Callbacks are regular C++ callables. Interactive low-level widgets also accept
callbacks directly:

```cpp
widget::button("Save", [&] { save_document(); }, ButtonVariant::Primary);
widget::checkbox("Pinned", pinned, [&] { pinned = !pinned; });
widget::text_field("Search", query, [&](std::string value) {
    query = std::move(value);
});
widget::tabs(items, selected, [&](std::size_t index) {
    selected = index;
});
```

This keeps the framework interface close to modern UI systems while staying
idiomatic for C++:

- SwiftUI-style component objects expose a single `body` function.
- Compose-style modifier ergonomics appear as fluent component methods and
  explicit layout wrappers.
- React-style stable identity is represented by call-site state and `key(...)`.
- Solid-style fine-grained state is expressed with typed `State<T>` and
  `Binding<T>` handles.
- C++ ownership stays explicit: app members, references, values, and lambdas
  decide where state lives.

Reference docs used for the interface direction:

- [SwiftUI: Declaring a custom view](https://developer.apple.com/documentation/swiftui/declaring-a-custom-view)
- [Jetpack Compose: Modifiers](https://developer.android.com/develop/ui/compose/modifiers)
- [Jetpack Compose: State hoisting](https://developer.android.com/develop/ui/compose/state-hoisting)
- [React: State as a snapshot](https://react.dev/learn/state-as-a-snapshot)
- [React: Preserving and resetting state](https://react.dev/learn/preserving-and-resetting-state)
- [Solid: Fine-grained reactivity](https://docs.solidjs.com/advanced-concepts/fine-grained-reactivity)

## Native Entry Points

```cpp
phenotype::native::run_app(1024, 720, "App", MyApp{});

phenotype::native::run_app(
    1024,
    720,
    "App",
    MyApp{},
    [](int width, int height, float scale) {
        cache_viewport(width, height, scale);
    });
```

Scene windows use the same component model:

```cpp
phenotype::native::scene_window::show(
    phenotype::NativeSceneWindowOptions{
        .identifier = "inspector",
        .title = "Inspector",
        .width = 360,
        .height = 480,
    },
    InspectorApp{});
```

## Build

```sh
mise install
mise exec -- intron install
mise exec -- exon build
```

## Verification

The core library is built with `exon`. Native examples and diagnostics can be
validated from their own worktrees or example directories with the same
`mise`-resolved `exon` toolchain.

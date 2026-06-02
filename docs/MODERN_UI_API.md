# Modern UI API

The phenotype UI interface is a C++23 component model built around:

- component objects with `body(ui::Context&)`
- typed local state via `ui::State<T>`
- explicit value bindings via `ui::Binding<T>`
- callback-based interaction
- stable identity through `key(...)` and call-site state
- native entry points that accept the same App object shape

## Component Shape

```cpp
struct SearchApp {
    phenotype::ui::View body(phenotype::ui::Context& ctx) {
        namespace ui = phenotype::ui;

        auto query = ctx.state<std::string>("query");
        auto pinned = ctx.state<bool>("pinned", false);

        return ui::VStack(
            ui::TextField("Search", query.binding()).width(320),
            ui::Button(pinned.get() ? "Unpin" : "Pin")
                .on_click([pinned] {
                    pinned.mutate([](bool& value) { value = !value; });
                }));
    }
};
```

`body` is a pure declaration pass from the framework perspective. A component
may keep ordinary C++ members, references, shared domain models, or state
holders; callbacks mutate those values and request a rebuild.

## State And Binding

```cpp
auto count = ctx.state<int>("count", 0);
count.set(10);
count.mutate([](int& value) { value += 1; });

auto name = ctx.state<std::string>("name");
ui::TextField("Name", name.binding());
```

`State<T>` is useful for UI-local values. `Binding<T>` is for controls that need
to write the full value, such as text fields.

## Low-Level Widgets

Low-level widget functions accept C++ callbacks directly:

```cpp
widget::button("Create", [&] { create_item(); });
widget::glass_button("Apply", [&] { apply_changes(); });
widget::checkbox("Enabled", enabled, [&] { enabled = !enabled; });
widget::radio("Compact", density == 0, [&] { density = 0; });
widget::switch_("Notifications", notifications, [&] {
    notifications = !notifications;
});
widget::text_field("Filter", filter, [&](std::string value) {
    filter = std::move(value);
});
widget::tabs(labels, selected, [&](std::size_t index) {
    selected = index;
});
```

The component wrappers are thin, composable conveniences over the same
callback-based primitives.

## Native Apps

```cpp
int main() {
    return phenotype::native::run_app(960, 640, "Search", SearchApp{});
}
```

Viewport changes can be handled as a plain callback:

```cpp
phenotype::native::run_app(
    960,
    640,
    "Search",
    SearchApp{},
    [](int width, int height, float scale) {
        record_viewport(width, height, scale);
    });
```

## Reference Direction

The interface follows current UI framework practices while preserving C++ value
semantics:

- [SwiftUI custom views](https://developer.apple.com/documentation/swiftui/declaring-a-custom-view): a component object declares its `body`.
- [Jetpack Compose modifiers](https://developer.android.com/develop/ui/compose/modifiers): visual and layout adjustments are explicit and ordered.
- [Jetpack Compose state hoisting](https://developer.android.com/jetpack/compose/state-hoisting): state can move up to a natural owner when reuse or coordination requires it.
- [React state snapshots](https://react.dev/learn/state-as-a-snapshot): each render observes a stable value snapshot.
- [React state identity](https://react.dev/learn/preserving-and-resetting-state): stable position and keys control state preservation.
- [Solid fine-grained reactivity](https://docs.solidjs.com/advanced-concepts/fine-grained-reactivity): small typed state handles keep updates local and direct.

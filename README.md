# phenotype

A cross-platform C++ UI framework with a Compose-style declarative DSL. Renders via WebGPU on the web, with plans to support native graphics APIs (Metal, Direct3D, Vulkan) on desktop and mobile.

## Example

```cpp
#include <string>
import phenotype;
using namespace phenotype;

auto CounterApp() {
    auto count = encode(0);

    Column([count] {
        Text("Hello, phenotype!");
        Text(std::string("Count: ") + std::to_string(count->value()));

        Row(
            [count] { Button("-", [count] { count->set(count->value() - 1); }); },
            [count] { Button("+", [count] { count->set(count->value() + 1); }); }
        );
    });
}

int main() {
    express(CounterApp);
    return 0;
}
```

```html
<script type="module">
    import { mount } from './phenotype.js';
    mount('counter.wasm');
</script>
```

## Key concepts

| Concept | Description |
|---|---|
| `express(fn)` | Application entry point — "expresses" the UI (genotype → phenotype) |
| `Trait<T>` | Reactive state holder — subscribers are notified on change |
| `encode(value)` | Create a persistent `Trait<T>` that survives mutations |
| `Column([&] { ... })` | Vertical flex layout (lambda builder for dynamic content) |
| `Row(...)` | Horizontal flex layout (same dual overload as Column) |
| `Text(str)` | Text display |
| `Button(str, callback)` | Clickable button with event handler |
| `Link(str, str)` | Hyperlink (opens URL on click) |
| `Code(str)` | Monospace code block with background |
| `Scaffold(top, content, bottom)` | Page layout with hero header, content area, and footer |
| `ListItems([&] { ... })` | Unordered list container |
| `Item(str)` | Bulleted list item |
| `Card([&] { ... })` | Rounded container with shadow |
| `Divider()` | Horizontal separator |
| `Spacer(px)` | Empty vertical space |

## Architecture

```
express(App)
  → Build pass: DSL builds layout tree (LayoutNode)
    → Layout pass: flexbox algorithm computes (x, y, w, h)
      → Paint pass: emit 2D draw commands to command buffer
        → flush()
          → JS WebGPU executor renders to canvas
```

### Rendering pipeline

1. **Build**: DSL calls (`Column`, `Text`, etc.) create `LayoutNode` objects in an arena allocator.
2. **Layout**: A flexbox subset algorithm computes pixel positions. Text measurement is delegated to JS via WASM import (`measureText`).
3. **Paint**: The layout tree is walked top-down, emitting draw commands (`FillRect`, `DrawText`, `RoundRect`, etc.) to a 256KB command buffer.
4. **Execute**: The JS shim reads the command buffer. Colored quads are rendered via instanced WebGPU draw calls. Text is rendered to an offscreen Canvas2D atlas, uploaded as a GPU texture, and drawn as textured quads.

### Draw commands

| Command | Description |
|---|---|
| `Clear` | Clear canvas with background color |
| `FillRect` | Filled rectangle |
| `StrokeRect` | Outlined rectangle |
| `RoundRect` | Rounded corner rectangle |
| `DrawText` | Text at position with font size and color |
| `DrawLine` | Line segment |
| `HitRegion` | Invisible click/hover target for event handling |

### Design principles

- **No external libraries**: Everything from scratch — layout, rendering, event handling.
- **Platform-agnostic C++ API**: All platform-specific logic lives in the JS shim. Native backends (Metal, Direct3D, Vulkan) will replace only the shim layer.
- **Compose-style DSL**: Implicit scope stack — `Column([&] { Text("hi"); })` automatically parents Text under Column.
- **Partial mutation**: `Trait::set()` rebuilds only subscribed scopes, not the entire tree.
- **C++23 modules**: Uses `.cppm` — no headers, no `#include`.

## Requirements

- [exon](https://github.com/misut/exon) (C++ package manager)
- [intron](https://github.com/misut/intron) (toolchain manager) or wasi-sdk installed manually

## Quick start

```bash
cd examples/hello
exon build --target wasm32-wasi
cp .exon/wasm32-wasi/debug/hello hello.wasm
python3 -m http.server 8080
# Open http://localhost:8080
```

As exon dependency:
```toml
[dependencies]
"github.com/misut/phenotype" = "0.4.1"
```

## Roadmap

- [x] WebGPU renderer (WGSL shaders, instanced draws, text atlas)
- [x] Compose-style declarative DSL
- [x] Layout engine (flexbox subset: column, row, gap, padding, max-width)
- [x] Text wrapping (word-wrap + newline support)
- [x] Reactive state (`Trait<T>`) with partial mutation
- [x] Theme system (design tokens for colors, fonts, spacing)
- [x] Event handling (click, pointer cursor)
- [x] Scroll and resize support
- [x] Hover states (visual feedback on Button/Link)
- [x] Text input (TextField component with caret, placeholder)
- [x] Keyboard navigation (Tab/Enter, focus ring)
- [ ] Custom theming API
- [ ] macOS (Metal backend)
- [ ] Windows (Direct3D backend)
- [ ] Linux (Vulkan backend)
- [ ] Android / iOS

## License

MIT

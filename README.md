# phenotype

A cross-platform C++ UI framework with a Compose-style declarative DSL. Currently targets the web via wasi-sdk + browser shim, with plans to support native platforms (Windows, macOS, Android, etc.) in the future.

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
| `Column(child1, child2)` | Vertical flex layout (variadic for static content) |
| `Row(...)` | Horizontal flex layout (same dual overload as Column) |
| `Text(str)` | Text display |
| `Button(str, callback)` | Clickable button with event handler |

## Architecture

```
express(App)
  → Scope stack + App() execution
    → Command buffer (phenotype low-level API)
      → flush()
        → JS DOM executor
```

- **Declarative DSL**: Compose-style implicit scope stack. `Column([&] { Text("hi"); })` automatically parents Text under Column.
- **Partial mutation**: When `Trait::set()` is called, only the subscribed scopes rebuild — not the entire UI tree.
- **Command buffer**: C++ writes DOM commands to WASM linear memory, JS reads and executes them in one batch.
- **Platform-agnostic C++ API**: Platform-specific logic lives in the shim layer only, enabling future native backends.
- **C++ modules**: Uses `.cppm` modules — supported by wasi-sdk's Clang.

## Requirements

- [exon](https://github.com/misut/exon) (C++ package manager)
- [intron](https://github.com/misut/intron) or wasi-sdk installed manually

## Quick start

```bash
cd examples/hello
exon build --target wasm32-wasi
cp .exon/wasm32-wasi/debug/hello hello.wasm
python3 -m http.server 8080
# Open http://localhost:8080/examples/hello/index.html
```

## Roadmap

- [x] Web (wasi-sdk + browser WASI shim)
- [x] Compose-style declarative DSL
- [x] Reactive state (`Trait<T>`) with partial mutation
- [x] Event handling (click)
- [ ] Modifier chains (padding, background, etc.)
- [ ] More event types (input, hover, etc.)
- [ ] Windows (native backend)
- [ ] macOS (native backend)
- [ ] Android (native backend)

## License

MIT

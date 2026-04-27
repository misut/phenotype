# phenotype

A cross-platform C++ UI framework with a declarative DSL inspired by Iced and
Elm. Single source of truth, typed messages, pure-function view. Renders via
WebGPU on the web today; native desktop support is split into a shared shell
plus platform-specific text / renderer adapters so macOS and Windows can
evolve independently without changing the core.

## Example

```cpp
#include <concepts>
#include <string>
#include <type_traits>
#include <variant>
import phenotype;

// 1. Messages — every event the UI emits.
struct Increment {};
struct Decrement {};
using Msg = std::variant<Increment, Decrement>;

// 2. State — single source of truth, owned by the framework runner.
struct State { int count = 0; };

// 3. update — pure transformation, the only place state mutates.
void update(State& state, Msg msg) {
    std::visit([&](auto const& m) {
        using T = std::decay_t<decltype(m)>;
        if constexpr (std::same_as<T, Increment>) state.count += 1;
        else if constexpr (std::same_as<T, Decrement>) state.count -= 1;
    }, msg);
}

// 4. view — pure function from state to a UI tree.
void view(State const& state) {
    using namespace phenotype;
    layout::column([&] {
        widget::text("Counter");
        widget::text(std::string("Count: ") + std::to_string(state.count));
        layout::row(
            [&] { widget::button<Msg>("-", Decrement{}); },
            [&] { widget::button<Msg>("+", Increment{}); }
        );
    });
}

int main() {
    phenotype::run<State, Msg>(view, update);
    return 0;
}
```

```html
<script type="module">
    import { mount } from './phenotype.js';
    mount('app.wasm');
</script>
```

`mount(wasmUrl, rootElement?, extraImports?)` accepts an optional third
argument — either an import-namespace object or a factory
`({ getMemory }) => namespaces` — so downstream projects can plug in
their own host APIs without forking the shim. See
[codon](https://github.com/misut/codon) for an example that streams a
generated `book.json` into the reader through this hook.

## Programming model

phenotype follows the Iced/Elm pattern: state lives in one place, messages are
the only way to mutate it, the view function is pure, and the framework owns
the rebuild loop.

| Step | Role |
|---|---|
| `Msg` | A `std::variant` (or any type) listing every event the app emits |
| `State` | The application's data, owned by the framework runner |
| `update(State&, Msg)` | Pure transformation — the only place state changes |
| `view(State const&)` | Pure function that builds the UI tree from state |
| `run<State, Msg>(view, update)` | Installs the runner; calls `view` once, then again after every dispatched message |

Widgets that emit events (`button`, `text_field`) are templated on `Msg` and
post a value to the queue when triggered. The runner drains the queue, folds
through `update`, then re-runs `view` to rebuild the layout tree.

## DSL reference

### Layout containers (`namespace layout`)

| Function | Description |
|---|---|
| `column(builder)` / `column(a, b, ...)` | Vertical flex container |
| `row(builder)` / `row(a, b, ...)` | Horizontal flex container, defaults to centered cross-axis so inline children share a baseline |
| `box(builder)` / `box(a, b, ...)` | Generic single-purpose wrapper, no direction or gap |
| `scaffold(top, content, bottom)` | Page layout with hero header, max-width content, and footer |
| `card(builder)` | Rounded white container with padding |
| `list_items(builder)` + `item(str)` | Bulleted list |
| `divider()` | 1px horizontal rule |
| `spacer(px)` | Fixed-height vertical gap |

### Widgets (`namespace widget`)

| Function | Description |
|---|---|
| `text(str)` | Plain text label |
| `code(str)` | Monospaced block with background, border, and padding |
| `link(label, href)` | Hyperlink that opens via the host JS shim |
| `button<Msg>(label, msg)` | Clickable button that posts `msg` on click |
| `text_field<Msg>(hint, current, mapper)` | Text input that maps each new value through `mapper` to a `Msg` |

### Application entry (`namespace phenotype`)

| Function | Description |
|---|---|
| `run<State, Msg>(view, update)` | Installs the runner and triggers the initial render |

### Diagnostics (`namespace phenotype::log`, `phenotype::metrics`, `phenotype::diag`)

OpenTelemetry-shaped logs and metrics are wired into the framework hot path
(`phenotype.diag`). Logs go to stderr (routed to `console.error` by the WASI
`fd_write` polyfill, no new host import needed). Counters / Gauges / Histograms
cover arena allocation, dispatch queue depth, runner phase timing, host
`measure_text` call counts, and input events. JS can call
`phenotype_diag_export()` to stage an OTLP-shaped JSON snapshot in shared
linear memory and forward it to an OTel collector via a separate adapter.
See `src/phenotype_diag.cppm` for the full instrument list and JSON shape, and
[docs/DEBUG_WORKFLOW.md](docs/DEBUG_WORKFLOW.md) for the shared debug plane,
artifact bundle layout, and platform-specific runtime extensions.

## Custom theming

Every phenotype app starts with the default `Theme` (design tokens
tuned for the docs site). To override any of them — colors, font
sizes, padding — call `phenotype::set_theme(...)` before `run<>()`,
or from inside `update()` for dynamic theme switching:

```cpp
int main() {
    phenotype::set_theme({
        .background = {10,  10,  10, 255},
        .foreground = {240, 240, 240, 255},
        .accent     = {100, 150, 255, 255},
        // any fields you omit keep their default value
    });
    phenotype::run<State, Msg>(view, update);
    return 0;
}
```

`set_theme` never triggers a rebuild on its own — the next rebuild
picks up the new values automatically because paint / layout /
widget code all read the theme directly. If you change the theme
from a non-message path (timer, external event), post a no-op
`Msg` afterwards to force a redraw.

`phenotype::current_theme()` reads the active theme from inside your
view function when you need to compute a derived color or pass a
palette to a helper.

## Architecture

```
phenotype::run<State, Msg>(view, update)
  └─ runner loop:
     ├─ drain message queue
     ├─ fold via update(state, msg)
     ├─ reset arena, run view(state)         ──┐
     ├─ layout pass (flexbox subset)            ├── instrumented as
     ├─ paint pass (emits draw commands)        │   phenotype.runner.phase_duration
     └─ flush() ──► JS WebGPU executor       ──┘
```

### Rendering pipeline

1. **Build** — DSL calls (`layout::column`, `widget::text`, …) allocate
   `LayoutNode`s in a generational arena. Stale `NodeHandle`s from prior
   rebuilds fail validation at deref time instead of dangling.
2. **Layout** — A flexbox subset (column / row / gap / padding / max-width /
   main-axis and cross-axis alignment) computes pixel positions. Text
   measurement is delegated to JS via the `measure_text` host import.
3. **Paint** — The layout tree is walked top-down, emitting draw commands
   (`Clear`, `FillRect`, `StrokeRect`, `RoundRect`, `DrawText`, `DrawLine`,
   `HitRegion`) into a 64KB shared linear-memory command buffer.
4. **Execute** — The JS shim parses the buffer. Colored quads render via
   instanced WebGPU draws; text is rasterized into an offscreen Canvas2D
   atlas, uploaded as a GPU texture, and drawn as textured quads.

### Design principles

- **Iced/Elm message DSL** — single source of truth, typed messages, pure
  view, pure update. No reactive primitives, no observer chains.
- **No external libraries** — layout, rendering, event handling, JSON, and
  diagnostics are all implemented from scratch.
- **Platform-agnostic C++ API** — the web path goes through the JS shim,
  while desktop native paths use the shared GLFW shell plus platform-specific
  text / renderer adapters behind the same command-buffer contract.
- **C++23 modules** — `.cppm` files, `import std;` (or GMF on wasi-sdk).
- **OTel-shaped observability** — diagnostics are framed as Counter / Gauge /
  Histogram + Severity 1/5/9/13/17/21 from day one so an external adapter can
  forward them to OTel collectors without changing the wasm side.

## Requirements

- [exon](https://github.com/misut/exon) (C++ package manager)
- [intron](https://github.com/misut/intron) (toolchain manager — installs
  llvm, cmake, ninja, wasi-sdk, wasmtime) or those tools installed manually

## Quick start

```bash
cd examples/native
exon run
```

`examples/native` is the desktop widget showcase — a non-trivial composition
of every shared widget rendered via the GLFW shell + Metal (macOS) or
Direct3D 12 (Windows) backend.

```bash
cd examples/workbook
exon run
```

`examples/workbook` is a product-style revenue forecast workbook. It combines
fixed-grid layout, keyed cells, text input, formula validation, dependency
recompute, selected-cell editing, audit state, and a canvas trend chart in one
runnable native example.

```bash
cd examples/flight_board
exon run
```

`examples/flight_board` is a visual flight operations dashboard. It validates
canvas route motion, generated media-like destination visuals, compact status
icons, state transitions, selected-flight synchronization, activity-feed
updates, and timeline progress in one runnable native example.

As an exon dependency:

```toml
[dependencies]
"github.com/misut/phenotype" = "0.13.0"
```

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the rendering pipeline,
host interface, command buffer protocol, native backend structure, and module
dependency graph. See [docs/DEBUG_WORKFLOW.md](docs/DEBUG_WORKFLOW.md) for the
unified debug workflow and cross-platform snapshot contract.

## Roadmap

### Done

- [x] WebGPU renderer (WGSL shaders, instanced draws, text atlas)
- [x] Iced/Elm message-based declarative DSL
- [x] Layout engine (flexbox subset: column / row / gap / padding / max-width / alignment)
- [x] Text wrapping (word-wrap + newline support)
- [x] Theme system (design tokens for colors, fonts, spacing)
- [x] Event handling (click, hover, pointer cursor)
- [x] Scroll and resize support with viewport culling
- [x] Hover states (visual feedback on `widget::button` and `widget::link`)
- [x] Text input (`widget::text_field` with caret, placeholder, native OS IME composition)
- [x] Keyboard navigation (Tab/Enter, focus ring)
- [x] OpenTelemetry-shaped logs and metrics (`phenotype.diag`)
- [x] Host `measure_text` cache (cross-rebuild memoization keyed by font size + content)
- [x] Custom theming API (runtime-configurable `Theme` via `phenotype::set_theme` / `current_theme`)
- [x] Theme-aware cache invalidation (`set_theme` clears the `measure_text` cache)
- [x] Image / icon rendering (`widget::image` + persistent atlas in the JS shim)
- [x] Checkbox / radio widgets (`widget::checkbox<Msg>` / `widget::radio<Msg>` — filled accent indicator + label, click + Tab focus on the indicator)
- [x] Theme from JSON (`theme_from_json` / `theme_to_json` via txn auto-reflection + `setThemeJson()` JS shim method)
- [x] OTel JS adapter (`shim/phenotype-otel.js`) for exporting metrics snapshots to OTLP/HTTP
- [x] vDOM-style diff / partial paint v1 — frame skip on identical cmd buffers
- [x] vDOM-style diff / partial paint v2 — double-buffer arena + position-based subtree diff
- [x] Native backend split (public coordinator + shared shell + platform modules)
- [x] macOS native backend (GLFW shell + CoreText + Metal)
- [x] Windows native backend (shared shell + DirectWrite + Direct3D 12)
- [x] Windows native IME composition and candidate overlay
- [x] Windows native image rendering (`DrawImage` local files + async remote images via WIC)
- [x] `examples/native` as a Windows acceptance showcase for text, IME, local/remote images, scroll, and interaction
- [x] Linux desktop stub backend
- [x] Docs package no longer syncs a root `CMakeLists.txt`

### Next Up

- [x] Windows native renderer / text stack — DirectWrite text measurement/atlas + Direct3D 12 renderer + WARP smoke coverage
- [x] OS-native URL opener on Windows — ShellExecuteW-backed native link opening
- [x] Broader non-macOS native contract tests — Windows native text/renderer/text-field/local+remote-image coverage
- [ ] macOS native IME composition — Windows is implemented, macOS still needs parity work
- [x] macOS native image rendering (`DrawImage`) — local files and async remote images are supported
- [ ] vDOM-style diff v3 — stable-key structural diff for efficient list reorder + sub-frame partial GPU updates
- [ ] Multi-line text input (`widget::text_area<Msg>`) — a high-value feature that builds directly on the current input model
- [ ] Spans / traces (`phenotype::diag::trace::Span`) — needed once native performance work moves beyond basic histograms
- [ ] Histogram exemplars — useful once slow-frame tracing exists and can be attached to exported metrics

### Later

- [ ] Animation system (transitions, easings, animated layout values) — best tackled after keyed diff and frame scheduling needs are clearer
- [ ] More widgets (slider, dropdown, modal/dialog, tooltip)
- [ ] Linux native renderer / text stack — useful after the Windows path proves out the current platform contracts
- [ ] Android / iOS

## License

MIT

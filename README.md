# phenotype

A cross-platform C++ UI framework with a declarative DSL built on a single
source of truth, typed messages, and a pure-function view. Renders via
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

phenotype follows a message-driven model: application state lives in one
place, messages are the only way to mutate it, the view function is pure, and
the framework owns the rebuild loop.

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
| `sized_box(max_width, builder)` | Fixed-width wrapper for layout slots that need an explicit cell width |
| `weighted(grow, builder)` | Row child that claims `grow / Σgrow` of the leftover width — multiple `weighted` siblings get a deterministic proportional split |
| `grid(columns, row_height, builder)` | Rigid `columns × N` grid; children are placed row-major into fixed tracks |
| `scaffold(top, content, bottom)` | Page layout with hero header, max-width content, and footer |
| `card(builder)` | Rounded white container with padding |
| `material_surface(kind, builder)` | Debug-visible material container with clear/thin/regular/thick semantics, macOS sampled-backdrop rendering through pure sampling-kernel and luminance-curve contracts, and resolved deterministic fallback plans elsewhere |
| `scroll_view(fixed_height, builder)` | Per-node scroll viewport that catches wheel events inside its bounds |
| `overlay(builder)` | Top-of-stack layer that paints after the main tree (foundation for dialogs, popovers, tooltips) |
| `dialog(builder, max_width=360, top_padding=96)` | Centered modal card on top of an `overlay` |
| `accordion(title, builder)` | Collapsible section; expand state lives in `framework_local<bool>` keyed to the call site |
| `list_items(builder)` + `item(str)` | Bulleted list |
| `divider()` | 1px horizontal rule |
| `spacer(px)` | Fixed-height vertical gap |

### Widgets (`namespace widget`)

| Function | Description |
|---|---|
| `text(str)` | Plain text label |
| `code(str)` | Monospaced block with background, border, and padding |
| `link(label, href)` | Hyperlink that opens via the host shim — text colour cross-fades on hover |
| `image(url, width, height)` | Async image; the JS shim or native backend keeps a persistent atlas |
| `canvas(width, height, paint, on_gesture?)` | Fixed-size leaf for arbitrary 2D drawing via a `Painter` callback |
| `button<Msg>(label, msg, variant?, disabled?)` / `ButtonStyleOptions` | Clickable button that posts `msg` on click; background and focus ring fade on hover / focus, with optional explicit chrome for app-like controls |
| `checkbox<Msg>(label, checked, msg)` / `radio<Msg>(label, selected, msg)` | Selection controls with a hover highlight on the row and a halo on focus |
| `switch_<Msg>(label, on, msg)` | Labelled on/off toggle; the thumb slides and the track cross-fades on flip |
| `tabs<Msg>(items, selected, on_select, TabsStyleOptions?)` | Material-backed segmented row with a 2 px sliding indicator under the selected tab |
| `progress(value, max_width?)` | Determinate progress bar |
| `progress_indeterminate(max_width?)` | Looping progress bar — a slug oscillates left↔right while the widget is on screen |
| `text_field<Msg>(hint, current, mapper, error?, disabled?)` / `TextFieldStyleOptions` | Text input that maps each new value through `mapper` to a `Msg`; glass search fields can opt into a semantic material plan |

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

## Animations

Visual transitions go through a small set of view-time primitives. They
take a `target` value plus a duration and return the current
interpolated value to bind into a node property:

```cpp
node.background  = animate_color(is_hovered ? hover_bg : base_bg, 150);
node.border_width = animate_float(is_focused ? 2.0f : 1.0f, 150);
```

Per-call-site state (the previous value, the active target, the start
timestamp) lives in `framework_local`, keyed off
`std::source_location` plus a per-Scope sibling counter, so two
buttons in the same row animate independently with no boilerplate.
When a target changes mid-flight the current interpolated value is
captured as the new start, so an interruption slides smoothly into
the new direction instead of snapping.

While any interpolation is still in progress the runner sets
`g_app.has_active_animations`. The host loop reads the flag and
schedules another `trigger_rebuild` ~16 ms later, which re-runs view
so `animate_*` advances. The flag self-clears every view, so the
loop drops back to its idle wait once everything converges.

The interactions wired through this pipeline:

- Hover background / text colour fades on `button`, `link`,
  `checkbox`, `radio`.
- Focus ring grow / fade across every focusable widget — the width
  expands to `theme.state_focus_ring_width` and the colour fades
  between the resting border and `theme.state_focus_ring`.
- `switch_` thumb slide + track cross-fade.
- `tabs` 2 px indicator that slides under the selected tab.
- `progress_indeterminate` looping slug.

Hover, focus, and scroll dispatches in the native desktop shells route through
`trigger_rebuild` (instead of just `repaint_current`) so each
transition is observed at view time. Scroll stays on the cheaper
paint-only path while no animation is running, then promotes to a
rebuild while the auto-tick is asking for ticks.

For the per-call-site keying mechanics, the cross-scope collision
caveat (sibling scopes today seed `widget_id_seed = 0`, so two
widgets at the same call counter inside different parent scopes
share state), and the paint-cache invalidation that lets fade-outs
finish, see the **View-time animation** section in
[docs/ARCHITECTURE.md](docs/ARCHITECTURE.md).

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

- **Message-driven DSL** — single source of truth, typed messages, pure
  view, pure update. No reactive primitives, no observer chains.
- **No external libraries** — layout, rendering, event handling, JSON, and
  diagnostics are all implemented from scratch.
- **Platform-agnostic C++ API** — the web path goes through the JS shim,
  while desktop native paths use AppKit / Win32 shells plus platform-specific
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
of every shared widget rendered via the AppKit + Metal (macOS) or Win32 +
Direct3D 12 (Windows) backend.

```bash
cd examples/glass_showcase
exon run
```

`examples/glass_showcase` is the material and debug-artifact scene. It renders
deterministic backdrop regions, all public material kinds, macOS sampled
backdrop material for functional-layer probes, resolved runtime material plans,
fallback metadata, and stable semantic labels for pixel-region verification.
Unsupported renderers still emit the same material plan schema with a
translucent fallback pass so artifacts explain why backdrop sampling did not
run.

```bash
cd examples/file_explorer_desktop
exon run
```

`examples/file_explorer_desktop` is a Finder-style desktop workflow with glass
toolbar/sidebar/icon-grid/status surfaces, document thumbnails, and sandboxed
temp-root file view, read, create, and delete actions.

```bash
cd examples/file_explorer_mobile
exon run
```

`examples/file_explorer_mobile` is the compact mobile version of the same
file-management model, with browse, preview, and create flows over the same
sandboxed data shape.

As an exon dependency:

```toml
[dependencies]
"github.com/misut/phenotype" = "0.13.0"
```

## Architecture

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the rendering pipeline,
host interface, command buffer protocol, native backend structure, and module
dependency graph. See [docs/DEBUG_WORKFLOW.md](docs/DEBUG_WORKFLOW.md) for the
unified debug workflow and cross-platform snapshot contract. See
[docs/APPLE_GLASS_GUI_ROADMAP.md](docs/APPLE_GLASS_GUI_ROADMAP.md) for the
Apple-style material GUI roadmap, current capability audit, and example/debug
coverage criteria. See [docs/EXAMPLES_COVERAGE.md](docs/EXAMPLES_COVERAGE.md)
for the current feature-to-example coverage matrix.

## Roadmap

### Done

- [x] WebGPU renderer (WGSL shaders, instanced draws, text atlas)
- [x] Message-based declarative DSL
- [x] Layout engine (flexbox subset: column / row / gap / padding / max-width / alignment)
- [x] Text wrapping (word-wrap + newline support)
- [x] Theme system (design tokens for colors, fonts, spacing)
- [x] Event handling (click, hover, pointer cursor)
- [x] Scroll and resize support with viewport culling
- [x] Hover states with view-time colour fades (`widget::button`, `widget::link`, `widget::checkbox`, `widget::radio`, `widget::tabs`)
- [x] Text input (`widget::text_field` with caret, placeholder, native OS IME composition)
- [x] Keyboard navigation (Tab/Enter, animated focus ring grow/fade across every focusable widget)
- [x] View-time animation primitives (`animate_color`, `animate_float`, `animate_value<T>`) backed by `framework_local` per-call-site state
- [x] Animation auto-tick — host loop schedules ~16 ms `trigger_rebuild` callbacks while `g_app.has_active_animations` is set; paint subtree cache invalidates whenever a view-time interpolation is in flight so fade-outs finish
- [x] Animated widgets — `widget::switch_` thumb slide + track cross-fade, `widget::tabs` material-backed sliding indicator with a raised-slot active style, `widget::progress_indeterminate` looping slug
- [x] Layout primitives (`overlay`, `dialog`, `scroll_view`, `accordion`, `weighted`, `sized_box`, `grid`)
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
- [x] macOS native backend (AppKit shell + CoreText + Metal)
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

- [ ] Animation easings beyond linear interpolation (cubic / spring / overshoot) and animated layout values like Column gap / row main_align
- [ ] `framework_local` Scope seeding — thread `parent_id` into `Scope` construction so siblings in different parent scopes stop colliding at `widget_id_seed = 0`
- [ ] Accordion body height-clip animation + chevron rotation
- [ ] More widgets (slider, dropdown, tooltip)
- [ ] Linux native renderer / text stack — useful after the Windows path proves out the current platform contracts
- [ ] Android / iOS

## License

MIT

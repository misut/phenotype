# Architecture

## Overview

phenotype = **platform-agnostic core** (C++23 modules) + **platform backends**.

The core handles widgets, layout, state management, vDOM diff, and draw-command emission. It knows nothing about windows, GPUs, or fonts — those are provided by the backend via a thin host interface.

- **Core modules**: types, state, layout, paint, commands, diag, theme_json
- **Backend contract**: `phenotype_host.h` (5 host functions) + `phenotype.commands` (typed draw commands)
- **Current backends**: WASM+JS (production), macOS native (GLFW shell + CoreText + Metal), Windows native (GLFW shell + DirectWrite + Direct3D 12), Linux stub native backend
- **Planned backends**: platform-specific native renderers/text systems behind the same shell + command-buffer contract

## Rendering pipeline

```
State change (message dispatch)
  │
  ▼
View function builds layout tree (Arena + LayoutNode)
  │
  ▼
vDOM diff v2 — copies layout for unchanged subtrees
  │
  ▼
layout_node() computes positions (skips layout_valid nodes)
  │
  ▼
paint_node() emits draw commands into cmd buffer (65 KB linear memory)
  │
  ▼
flush_if_changed() — FNV-1a hash, skip if identical to previous frame
  │
  ▼
Backend consumes the cmd buffer:
  ├── WASM:   JS shim parseCommands() → WebGPU renderer
  └── Native: phenotype::parse_commands() → platform renderer
```

## Host interface

`src/phenotype_host.h` declares 5 `extern "C"` functions that every backend implements:

| Function | Role |
|---|---|
| `phenotype_flush()` | Signal the renderer to consume the cmd buffer |
| `phenotype_measure_text(font_size, mono, text, len)` | Return pixel width of a text string |
| `phenotype_get_canvas_width()` | Viewport width in pixels |
| `phenotype_get_canvas_height()` | Viewport height in pixels |
| `phenotype_open_url(url, len)` | Open a URL in the browser or OS |

On WASM, a `PHENOTYPE_IMPORT` macro applies `__attribute__((import_module, import_name))` so the JS shim resolves them at link time. On native, the macro is empty and the backend provides implementations as regular C functions.

## Command buffer protocol

The cmd buffer (`phenotype_cmd_buf[65536]`) uses a binary encoding with 8 opcodes:

| Opcode | Name | Payload (after 4-byte opcode) |
|--------|------|------|
| 1 | Clear | u32 color |
| 2 | FillRect | f32 x, y, w, h; u32 color |
| 3 | StrokeRect | f32 x, y, w, h, line_width; u32 color |
| 4 | RoundRect | f32 x, y, w, h, radius; u32 color |
| 5 | DrawText | f32 x, y, font_size; u32 mono, color, len; bytes text (4-byte aligned) |
| 6 | DrawLine | f32 x1, y1, x2, y2, thickness; u32 color |
| 7 | HitRegion | f32 x, y, w, h; u32 callback_id, cursor_type |
| 8 | DrawImage | f32 x, y, w, h; u32 len; bytes url (4-byte aligned) |

All values are little-endian. Colors are packed as `(r << 24) | (g << 16) | (b << 8) | a`.

The `phenotype.commands` module provides a C++ parser (`parse_commands(buf, len)`) that decodes these bytes into typed structs (`ClearCmd`, `FillRectCmd`, etc.) for native backends.

## Native backend structure

The native path is intentionally split into three layers so macOS and Windows work mostly in separate files:

1. **`phenotype.native.shell`** — the shared GLFW shell: window lifecycle, input callbacks, scroll/focus/hover translation, event loop
2. **`phenotype.native.platform`** — capability contracts for `text`, `renderer`, and platform services
3. **Platform implementation modules** — `phenotype.native.macos`, `phenotype.native.windows`, `phenotype.native.stub`

This keeps the framework core pure while pushing side effects into thin adapters.

## Unified debug plane

Debugging follows one shared model across native and WASI targets:

- the common snapshot schema is defined in `phenotype` / `phenotype.diag`
- platform adapters only provide capability overrides and `platform_runtime.details`
- artifact bundles always start from `snapshot.json`, then add optional
  `frame.bmp` and `platform/<platform>-runtime.json`

See [DEBUG_WORKFLOW.md](DEBUG_WORKFLOW.md) for the full contract and the current
macOS / WASI extensions.

### Current native backends

- **macOS**: GLFW shell + CoreText text measurement/atlas + Metal renderer + native `DrawImage` for local files and async remote images
- **Windows**: GLFW shell + DirectWrite text measurement/atlas + Direct3D 12 renderer + IME composition overlay + native `DrawImage` for local files and async remote images
- **Android**: GameActivity-driven shell (`examples/android/`) + Vulkan renderer that clears the surface to `theme.background` each frame. Text / input / debug are stubs (Stage 4+ replace them with JNI-backed Paint and AMOTION_EVENT routing). Emits `libphenotype-modules.a` via exon; the example Gradle project packages it into an APK together with `androidx.games:games-activity:3.0.5`.
- **Linux / other desktop**: shared stub backend

### Modularity guarantee

The core remains isolated from platform SDKs. Native implementations only meet two contracts:

1. **Core host capabilities** — measurement, canvas sizing, URL opening, command-buffer flush
2. **`phenotype.commands`** — typed draw commands parsed from the shared buffer

This means Metal, Direct3D, Vulkan, Skia, software raster, or another future renderer can replace a platform backend without rewriting `types`, `state`, `layout`, `paint`, or `diag`.

## Native backend roadmap

- [x] Host interface abstraction — `src/phenotype_host.h` (PR #52)
- [x] Command buffer C++ parser — `phenotype.commands` module (PR #53)
- [x] Shared GLFW shell extracted from platform-specific native code
- [x] macOS native code isolated behind a dedicated platform module
- [x] Windows backend wired into the native module graph
- [x] Native text measurement on Windows (`text_api`) via DirectWrite
- [x] Windows renderer implementation behind `renderer_api` via Direct3D 12
- [x] OS-native URL opener on Windows
- [x] IME composition for native text input on Windows
- [x] Native `DrawImage` local-file support on Windows
- [x] Native `DrawImage` async remote-image support on Windows
- [x] Contract tests for the Windows native backend (text, renderer smoke, text input, local/remote image upload paths)
- [x] `examples/native` positioned as the Windows native acceptance showcase
- [ ] IME composition for native text input on macOS
- [x] Native `DrawImage` support on macOS
- [x] Shell core / GLFW driver split (`phenotype.native.shell` + `phenotype.native.shell.glfw`) to unblock non-GLFW shells
- [x] `aarch64-linux-android` exon target with Android-stub platform (Stage 0)
- [x] Android Vulkan clear-color backend + GameActivity example (Stage 2)
- [ ] Android color primitives pipeline (`FillRect` / `StrokeRect` / `RoundRect` / `DrawLine`) — Stage 3
- [ ] Android text pipeline via JNI → `android.graphics.Paint` — Stage 4
- [ ] Android image pipeline — Stage 5
- [ ] Android touch / GameTextInput routing — Stage 6
- [ ] Android debug plane + device contract tests — Stage 7

## Module dependency graph

```
phenotype (umbrella re-export)
├── phenotype.types       — Color, Cmd, Style, LayoutNode, NodeHandle, Decoration
├── phenotype.state       — Arena, AppState, Scope, InputHandler, message queue
├── phenotype.diag        — OTel-shaped Counter, Gauge, Histogram, log ring, JSON snapshot
├── phenotype.layout      — flexbox engine, measure_text cache, vDOM diff
├── phenotype.paint       — draw command emitters, cmd buffer, flush_if_changed
├── phenotype.commands    — typed command structs (ClearCmd, ...) + parse_commands()
├── phenotype.theme_json  — Theme ↔ JSON via txn auto-reflection
├── phenotype.native      — public native entrypoint, platform selection
├── phenotype.native.shell — GLFW event loop + input translation
├── phenotype.native.platform — shared native capability contracts
├── phenotype.native.macos — macOS text + Metal renderer
├── phenotype.native.windows — Windows text + Direct3D 12 renderer
└── phenotype.native.stub — shared stub backend for non-macOS native targets
```

External dependencies:
- `jsoncpp` — JSON parse/emit (used by diag snapshot + theme_json)
- `txn` — serialization framework (used by theme_json)
- `cppx` — shared platform boundary helpers (HTTP, URL opening, Unicode/resource utilities) plus reflection used across native integration points

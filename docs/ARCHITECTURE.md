# Architecture

## Overview

phenotype = **platform-agnostic core** (C++23 modules) + **platform backends**.

The core handles widgets, layout, state management, vDOM diff, and draw-command emission. It knows nothing about windows, GPUs, or fonts — those are provided by the backend via a thin host interface.

- **Core modules**: types, state, layout, paint, commands, diag, theme_json
- **Backend contract**: `phenotype_host.h` (5 host functions) + `phenotype.commands` (typed draw commands)
- **Current backends**: WASM+JS (production), native test stubs (CI)
- **Planned backends**: Dawn/WebGPU (Windows, macOS, Linux)

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
  └── Native: phenotype::parse_commands() → Dawn / D3D / Vulkan renderer
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

## Native rendering strategy

### Decision: Google Dawn (WebGPU)

phenotype uses [Dawn](https://dawn.googlesource.com/dawn) — Google's C++ WebGPU implementation — as the initial native renderer.

**Why Dawn:**
- Reuses the existing WGSL shaders (~125 lines) from `shim/phenotype.js` as-is
- Cross-platform with a single codebase: D3D12 (Windows), Metal (macOS), Vulkan (Linux)
- Memory-safe by design: automatic resource lifetime management, implicit barrier insertion, built-in validation layer
- Proven at scale (powers Chrome's WebGPU)

### Hybrid approach (future-proofing for game engine use)

| Phase | Trigger | Action |
|-------|---------|--------|
| **Phase 1** (current) | Default | Dawn for all rendering. One WGSL shader set covers all platforms. |
| **Phase 2** | Performance ceiling hit (>1000 draw calls, need bindless) | Use Dawn's native handle interop (`dawn::native`) to drop into D3D12/Vulkan for specific render passes. Core stays on Dawn. |
| **Phase 3** | AAA-level requirements (mesh shaders, ray tracing, GPU-driven pipeline) | Swap the renderer entirely to raw D3D12/Vulkan. The core framework is untouched because it only depends on `phenotype_host.h` + `phenotype.commands`. |

### Modularity guarantee

The renderer is always behind two abstraction layers:

1. **`phenotype_host.h`** — 5 functions the renderer implements
2. **`phenotype.commands`** — typed draw commands the renderer consumes

Any renderer (Dawn, wgpu-native, raw D3D12, Metal, Vulkan, or a future GPU API) can be plugged in by implementing these two interfaces. The core framework modules (`types`, `state`, `layout`, `paint`, `diag`) never link against any GPU library.

### Known WebGPU limitations

These are acceptable for UI + 2D + indie 3D workloads. They become blocking only at AAA scale, at which point Phase 2 or Phase 3 kicks in.

- No bindless rendering (bind-group based)
- No mesh shaders
- No hardware ray tracing (proposal stage in the WebGPU spec)
- Limited multi-threaded command recording
- ~10-30% CPU-side overhead vs raw D3D12/Vulkan for >1000 draw calls per frame

## Native backend roadmap

- [x] Host interface abstraction — `src/phenotype_host.h` (PR #52)
- [x] Command buffer C++ parser — `phenotype.commands` module (PR #53)
- [ ] Dawn integration + prototype window
- [ ] Native text measurement (DirectWrite on Windows, CoreText on macOS, FreeType on Linux)
- [ ] Windowing + event loop (SDL3 or GLFW — decision depends on Dawn surface requirements)
- [ ] Native entry point (`phenotype::run_native`)

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
└── (future) phenotype.backend.dawn — Dawn renderer, native text, windowing
```

External dependencies:
- `jsoncpp` — JSON parse/emit (used by diag snapshot + theme_json)
- `txn` — serialization framework (used by theme_json)
- `cppx` — compile-time reflection (transitive via txn)

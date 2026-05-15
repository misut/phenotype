# Architecture

## Overview

phenotype = **platform-agnostic core** (C++23 modules) + **platform backends**.

The core handles widgets, layout, state management, vDOM diff, and draw-command emission. It knows nothing about windows, GPUs, or font files — those are provided by the backend via a thin host interface.

- **Core modules**: types, material, state, layout, paint, commands, diag, theme_json
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

## View-time animation

Visual transitions are bound at view time, not paint time. A widget
asks for a value via `animate_color` / `animate_float` /
`animate_value<T>` (all in `phenotype.cppm`); the call returns the
current interpolated value to drop into a `LayoutNode` field, and the
runtime advances the interpolation automatically across rebuilds:

```cpp
node.background  = animate_color(is_hovered ? hover : base, 150);
node.border_width = animate_float(is_focused ? 2.0f : 1.0f, 150);
```

### Per-call-site state via `framework_local`

The interpolation's previous value, active target, and start timestamp
live in `framework_local<AnimationState<T>>`, keyed off:

```
widget_id = hash(scope_seed,
                 hash(hash_source_location(loc),
                      hash(extra_key, per_scope_sibling_counter)))
```

- `loc` defaults to `std::source_location::current()` at the
  `animate_*` call site.
- `extra_key` is the `key` argument that propagates through to
  `framework_local` (most widgets pass `0`); it lets a caller
  disambiguate keyed list iterations or other repeats it can't avoid.
- `per_scope_sibling_counter` lives on the active `Scope`. Each new
  call at a given `loc` increments the counter, so two `widget::button`
  invocations in the same row pick distinct widget ids without any
  manual key.
- `scope_seed` is reserved for parent-driven seeding so distinct
  containers can produce non-colliding ids. **Today every container
  constructs its `Scope` with `widget_id_seed = 0`**, which means two
  widgets at the same call counter inside different parent scopes
  (e.g. the first button in two separate cards) currently share state.
  The same caveat applies to every other `framework_local` consumer
  (scroll offsets, accordion expand state, etc.). A future PR will
  thread the parent's id into the seed to remove the collision; until
  then, callers that need to avoid it pass an explicit `key` (typically
  the loop index when iterating).

When `target` changes mid-flight, `animate_value` captures the current
interpolated value as the new `start_value` so the new fade picks up
where the previous one was. That makes interrupting Tab cycles or
hover entry/exit feel continuous instead of snapping.

A non-inline `detail::steady_ms()` wraps the only `<chrono>` call, so
widget templates that instantiate `animate_value<T>` see a plain
`std::int64_t` interface without pulling chrono into their
translation unit (MSVC enforces module type reachability strictly
enough for this to matter; Clang was lenient).

### Auto-tick + the `has_active_animations` flag

While any interpolation has `progress < 1.0`, `animate_value` sets
`g_app.has_active_animations = true`. The runner clears the flag at
the start of every view, so it reads as "did this frame's view
request another tick?". The GLFW shell's main loop polls the flag and
re-enters `trigger_rebuild()` on a ~16 ms cadence whenever it stays
set; once everything converges the flag stays false and the loop
drops back to its idle wait. Other shells (Android, wasm) inherit the
flag mechanism via the runner; their host loops grow their own
auto-tick path as a follow-up.

`trigger_rebuild` (not `repaint_current`) is the load-bearing
choice — repaint just walks the existing arena, so without rebuilding
the view the baked-in interpolated value would freeze at the start of
the fade. The shell's hover, focus, and scroll dispatches all promote
to `trigger_rebuild` whenever a transition is in flight (scroll stays
on the cheaper paint-only path while idle so the steady-state cost is
unchanged).

### Paint cache invalidation while animating

`paint_invalidation_mask` normally only includes the callback ids
whose hover / focus state changed since the previous frame, so paint
can blit cached subtree bytes for everything else. That falls down
during a fade-out: the widget that just lost focus or hover is no
longer in the diff, but its `node.border_width` /
`node.background` are still interpolating each frame. The runner
forces `paint_invalidation_mask = ~0ULL` whenever
`has_active_animations` is set during view, so every cached subtree
re-walks until the animation lands. The flag self-clears, so the
diff-driven cache returns the moment everything converges.

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

The default proportional text stack prefers Pretendard and falls back to the
platform sans stack when the face is unavailable. Monospace text keeps the
platform monospace default. This keeps Korean, Chinese, and Japanese text closer
to the intended product typography without requiring every example to pass an
explicit `FontSpec`.

## Command buffer protocol

The cmd buffer (`phenotype_cmd_buf[65536]`) uses a binary encoding with
append-only opcodes:

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
| 15 | MaterialRect | f32 x, y, w, h, radius; u32 kind, role; u32 container_id, union_id; f32 container_spacing; u32 container_flags; f32 opacity, blur_radius; u32 tint; f32 saturation, luminance_floor, luminance_gain, edge_highlight, edge_width, noise_opacity, shadow_alpha, shadow_radius |

All values are little-endian. Colors are packed as `(r << 24) | (g << 16) | (b << 8) | a`.

The `phenotype.commands` module provides a C++ parser (`parse_commands(buf,
len)`) that decodes these bytes into typed structs (`ClearCmd`, `FillRectCmd`,
`MaterialRectCmd`, etc.) for native backends.

## Material Planning

Material policy lives in the pure `phenotype.material` module. Backends provide
immutable edge inputs — capability snapshot, backdrop descriptor, render-target
metadata, debug seed, and quality policy — then execute the returned
`MaterialPlan`.

`Cmd::MaterialRect` carries the material node's material command descriptor
across the backend boundary: kind, functional surface role, material container
identity, union identity, container spacing/flags, opacity, blur, tint,
saturation, luminance curve, edge highlight, edge width, noise opacity, and
shadow. In C++ this is represented as `MaterialCommandDescriptor`; backends
reconstruct `MaterialRequest` from that descriptor plus geometry, then call the
pure planner. They should not re-derive these style values, functional roles, or
container grouping from the current theme after the command has been emitted.

```cpp
MaterialPlan plan = plan_material_surface(request, environment);
```

The plan records its artifact `contract_version`, source
`command_descriptor`, material `role`, material container analysis, blur, tint,
saturation, luminance curve, edge highlight, noise/dither, shadow, render-target
analysis, backdrop sampling, backdrop analysis, decision trace, fallback path,
debug metadata, pass expectations, the resolved quality policy, resource
budgets, and verifier expectations.
`decision_trace` records the pure gate booleans for geometry, target readiness,
quality, backend capabilities, accessibility settings, backdrop-source
readiness, and the first fallback blocker. `primary_pass` states whether the
backend should run a backdrop
blur pass or deterministic translucent fallback. It also records the pure
executor role and maximum texture-copy pixels for that pass, so a backend
artifact can prove whether it stayed within the render-target copy budget.
`sample_taps` records the actual taps required by that resolved pass, so
deterministic fallback plans use `sample_taps: 0` even when the quality budget
allows more. The pure planner normalizes caller tap limits to executable
backdrop kernels of 1, 5, 9, 13, or 25 taps, selecting the largest kernel that
does not exceed `quality_policy.max_sample_taps`. Reduced-motion plans also
disable material noise and cap backdrop sample taps before any backend executes
the pass. `quality_policy` records the pure planner's resolved
sampling/noise/shadow switches and caller quality limits, including
`max_backdrop_pixels`. `render_target` records sanitized target dimensions,
scale, pixel format, pixel count, readiness, and whether the target stays
within that backdrop budget. `resource_budget` records the clamped
blur/executable sample-tap limits, the same allowed backdrop-pixel budget, and
whether texture copies and fallback behavior are bounded. Container spacing is
also reported as `max_container_spacing`, so artifact gates can bound future
container/union expansion work before a backend starts allocating extra
backdrop passes.
`verifier` records the deterministic pixel-region contract derived from the
same plan: whether a backdrop source or edge highlight must be present, the
minimum luma/color thresholds for sampled or fallback rendering, the semantic
profile name, whether material container identity/morph contracts are expected,
and the likely pass layer that a failure should inspect first.
`MaterialContainerAnalysis` records whether a surface is isolated, participates
in a named material container, or belongs to a shape union. Reduced Motion keeps
container identity intact but disables morph-transition expectations in the pure
plan, matching Apple's guidance to coordinate related glass surfaces while still
respecting accessibility settings.
When a stable backdrop descriptor is available, the pure planner also copies
its source, readiness flags, sanitized luminance statistics, response bucket,
and floor/gain/edge deltas into `MaterialPlan.backdrop`. The same value drives
glass luminance curve and edge-highlight adjustment before the plan reaches a
backend. This keeps legibility policy in the deterministic layer as blur, tint,
and fallback decisions, and lets artifacts explain why a material responded to
dark, bright, flat, or neutral backdrop content.
Artifact gates can separately bound actual plan taps and resource-budget taps,
which lets fallback scenes require zero executed taps while preserving the
backend's allowed quality budget in the same artifact.
If the quality policy disables backdrop sampling, reduces the blur/tap budget
to zero, or the render target exceeds `max_backdrop_pixels`, the pure planner
returns `fallback_path: quality-policy` instead of leaving the backend to infer
that downgrade.
macOS probes `NSWorkspace` accessibility display preferences at the native
edge and passes the immutable `reduce_transparency`, `increase_contrast`, and
`reduce_motion` booleans into `MaterialEnvironment`. The pure planner remains
the only layer that decides whether those inputs produce opaque fallback,
higher contrast, reduced noise, or lower sample taps. The runtime artifact also
records `renderer.accessibility_display_options` so a captured frame explains
which system or test override fed the plan.
Runtime adapters serialize the same `MaterialRuntimeRecord` shape into
`debug.platform_runtime.details.renderer.material_plans`: macOS records the
sampled-backdrop pass, Windows and Android record deterministic fallback
plans, and snapshot-only targets publish an empty renderer contract with an
explicit fallback policy. Each serialized plan carries the same
`contract_version` so artifact verifiers can reject unknown material plan
schemas before reading version-specific fields. The renderer object also carries
`material_plan_contract_version`, allowing empty renderer contracts to advertise
the same artifact schema. Backends also publish
`renderer.material_runtime_summary`, a flat count/max summary derived from
the same records; the artifact verifier recomputes it from
`material_plans[]` so CI can catch summary drift, unexpected executor pass
growth, and texture-copy budget drift.
Backends separately publish `renderer.material_executor_summary` for edge-only
execution telemetry: material instances, fallback instances, material draw
calls, upload bytes/capacity, framebuffer-history copy bounds, and CPU enqueue
timings. These fields are not inputs to the pure planner. They let artifact
gates prove the backend stayed inside deterministic resource limits while also
leaving nondeterministic timing data available for post-failure diagnosis.
Platform APIs, Metal/AppKit calls, shader compilation, texture capture, clocks,
filesystem writes, and process execution stay outside this pure layer.

Apple's Human Interface Guidelines distinguish Liquid Glass from standard
materials: Liquid Glass belongs primarily to functional control/navigation
layers that float above content. phenotype follows that boundary in its
contract language. Backends may render an Apple-inspired glass effect, but the
core material API remains a cross-platform semantic contract with explicit
fallbacks rather than a claim to reproduce private system component behavior.
The layout DSL exposes this boundary through `layout::material_surface` for
low-level material containers and `layout::toolbar`, `layout::sidebar`, and
`layout::status_bar` for common app chrome. Those helpers only configure
layout, semantic labels, and `MaterialSurfaceRole`; they still emit the same
`MaterialRect` command and flow through the pure planner/backend executor
contract above. Artifact gates can require roles such as `toolbar`, `sidebar`,
`status_bar`, `navigation`, or `surface` without changing backend rendering
policy.

## Native backend structure

The native path is intentionally split into three layers so macOS and Windows work mostly in separate files:

1. **`phenotype.native.shell`** — the shared GLFW shell: window lifecycle, input callbacks, scroll/focus/hover translation, event loop
2. **`phenotype.native.platform`** — capability contracts for `text`, `renderer`, and platform services
3. **Platform implementation modules** — `phenotype.native.macos`, `phenotype.native.windows`, `phenotype.native.stub`

This keeps the framework core pure while pushing side effects into thin adapters.

## Unified debug plane

Debugging follows one shared model across native and WASI targets:

- the common snapshot schema is defined in `phenotype` / `phenotype.diag`
- semantic snapshots include root-level overlays after the main tree with
  screen-fixed visibility, matching the paint and focus traversal order
- platform adapters only provide capability overrides and `platform_runtime.details`
- artifact bundles always start from `snapshot.json`, then add optional
  `frame.bmp` and `platform/<platform>-runtime.json`

See [DEBUG_WORKFLOW.md](DEBUG_WORKFLOW.md) for the full contract and the current
material/runtime extensions.

### Current native backends

- **macOS**: GLFW shell + CoreText text measurement/atlas + Metal renderer + native `DrawImage` for local files and async remote images. Empty non-monospace font requests prefer Pretendard before falling back to the system UI face.
- **Windows**: GLFW shell + DirectWrite text measurement/atlas + Direct3D 12 renderer + IME composition overlay + native `DrawImage` for local files and async remote images. Empty non-monospace font requests prefer Pretendard before falling back to Segoe UI.
- **Android**: GameActivity-driven shell (`examples/android/`) + Vulkan renderer with three instanced pipelines: a color pipeline (FillRect / StrokeRect / RoundRect / DrawLine / Clear — same `ColorInstance { rect; color; params }` layout and `params.z` draw-type dispatch as the macOS / Windows color pipelines); a text pipeline that samples an R8 atlas rasterised via JNI into `android.graphics.Paint` / `Canvas` / `Bitmap` (UTF-8 → UTF-16 via `cppx::unicode::utf8_to_utf16` before `JNIEnv::NewString`, with empty non-monospace requests preferring Pretendard and relying on Android's deterministic default fallback when unavailable); and an image pipeline that samples a persistent 2048² RGBA8 atlas strip-packed from images decoded via NDK's `AImageDecoder` (PNG / JPEG / WebP / GIF / HEIF). Image URLs resolve through `asset://path` (bundled assets via `AAssetManager`) or absolute filesystem paths (`file://` prefix stripped); `http(s)://` renders a placeholder until Stage 7. Input routes GameActivity's `android_input_buffer` (motionEvents + keyEvents + ACTION_SCROLL axes) into `phenotype::native::detail::dispatch_*` via the `phenotype_android_dispatch_{pointer,key,char,scroll}` C ABI; the Android renderer snapshots each flushed command buffer so `renderer.hit_test` can walk the `HitRegionCmd` list in reverse without forcing another view pass. The example driver boots a baked-in counter demo via `phenotype_android_start_app`, which instantiates `detail::run_host<demo6::State, demo6::Msg>` so view/update runs entirely in-library. GLSL sources live at `src/phenotype.native.android.shaders/{color,text,image}.{vert,frag}` and are precompiled to SPIR-V via `tools/compile_android_shaders.sh` (NDK's bundled `glslc`) into `src/phenotype.native.android.shaders.inl`, which the native module includes directly. `debug_api` is feature-parity with macOS / Windows: every `renderer_flush` copies the presented swapchain image into a persistent `debug_capture_image` so `capture_frame_rgba()` reads a fresh snapshot on demand; `snapshot_json` + `write_artifact_bundle` delegate to the shared `::phenotype::diag::detail` helpers. Resume handling clears `last_paint_hash` and calls `trigger_rebuild()` from `phenotype_android_attach_surface` so the first post-`APP_CMD_INIT_WINDOW` frame paints instead of staying black. Emits `libphenotype-modules.a` via exon; the example Gradle project packages it into an APK together with `androidx.games:games-activity:3.0.5` and links `libjnigraphics.so` for both `AndroidBitmap_*` zero-copy pixel reads and the `AImageDecoder_*` family.
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
- [x] Android color primitives pipeline (`FillRect` / `StrokeRect` / `RoundRect` / `DrawLine`) — Stage 3
- [x] Android text pipeline via JNI → `android.graphics.Paint` — Stage 4
- [x] Android image pipeline — Stage 5
- [x] Android touch / GameTextInput routing — Stage 6
- [x] Android debug plane + resume-flash fix + scroll routing — Stage 7
- [ ] Android device contract test runner (mirror Windows / macOS suites onto emulator) — follow-up
- [ ] Android async image decode + HTTPS remote fetch (worker thread + JNI HttpsURLConnection) — follow-up
- [ ] Android multi-touch gestures / pinch / rotate (shell-core dispatcher additions) — follow-up
- [ ] Android soft keyboard + GameTextInput + IME composition — follow-up

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

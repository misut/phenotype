# Runtime Architecture Direction

Phenotype started with a deliberately small Elm-style loop: one application
state, one message queue, one view rebuild, and one render target. That shape is
easy to teach, but it does not scale cleanly to desktop UI features such as
settings windows, debug panels, tool palettes, and future multi-document
windows. Those surfaces need independent input focus, hover state, message
queues, frame timelines, and renderer resources while still sharing application
services such as theme resolution and diagnostics.

The long-term runtime direction is a three-layer model:

- `ApplicationRuntime` owns process-wide services: platform application
  integration, theme and system-preference resolution, diagnostics, resource
  catalogs, shared caches, and command/menu registration.
- `SceneRuntime` owns one declarative UI root: app state, message queue,
  focus/hover/pressed state, framework-local storage, root tree, previous tree,
  debug overlay state, frame timeline, and scheduling policy.
- `RenderSurface` owns one native drawing target: window/view handle, drawable
  size, backend renderer state, hit regions, damage tracking, frame capture, and
  backend-specific resources.

This follows the same ownership split used by established UI systems:

- SwiftUI models application UI as `Scene`s such as `WindowGroup` and
  `Settings`, each with its own lifecycle.
- React attaches state to explicit roots and tree positions; independent roots
  should not share transient render state accidentally.
- Jetpack Compose keeps composables pure and hoists state to the lowest common
  owner, so frequently-changing local state does not force unrelated UI to
  rebuild.
- Desktop shells such as Electron and Tauri treat windows as addressable
  handles with separate lifecycle and focus behavior.

The first migration steps keep the existing `phenotype::run` API source
compatible, but internally route the active `AppState`, type-erased message
queue, and `framework_local` storage through a `SceneRuntime`. Native shell
state such as scroll position, last pointer position, drag selection, caret
blink, deferred input repaint, and repaint coalescing now lives on the
`native_host` that receives the event. This gives the codebase a safe place to
attach future settings windows without sharing hover, focus, queued messages,
scroll offsets, widget open/closed flags, or animation state with the main
window.

`RenderSurfaceRuntime` is the next foundation layer. It binds one native drawing
target to one `SceneRuntime`, records logical/framebuffer size, visibility, frame
sequence, and damage generation, and switches the active scene when a surface is
activated. Desktop `native_host` instances now synchronize their
`NativeSurfaceDescriptor` into this runtime and record frame completion after a
successful native present. Platform renderer backends still need to move their
Metal, D3D, and Vulkan resources into this surface owner before multiple native
phenotype windows can render independently, but the public runtime snapshots and
tests now make that ownership boundary explicit.

The shared debug plane publishes both runtime owners in
`debug.platform_runtime.scenes[]` and
`debug.platform_runtime.render_surfaces[]`. CLI, artifact, and side-panel tools
can now confirm which scene owns hover/focus/message state and which surface
owns size, visibility, damage, and frame counters before relying on
platform-specific renderer diagnostics.
Render surfaces also own the paint/flush cache key (`last_paint_hash`) and
flush/skip counters. The legacy `AppState::last_paint_hash` remains mirrored for
older tests and callers, but `flush_if_changed` compares against the active
surface so independent windows do not accidentally suppress each other's first
paint.

Each scene snapshot also carries a `schedule` object. It reports whether a
runner is installed, whether view-time animations, scrollbar animation,
input-motion, or debug-panel refresh are requesting future ticks, and how far
the frame trace/timeline counters have advanced. Native shells should query
these scene-schedule helpers instead of reading global flags directly, so a
future settings or debug window can request frames without waking unrelated
scenes.

Scene rebuild runners are stored as context-aware callbacks on `SceneRuntime`,
not on widget app state. The compatibility `run<State, Msg>` entry point still
installs one main-scene runner, but that runner now receives an explicit
context object containing its host, user state, view, and update closures, and
that context is owned by the scene rather than by a function-template static.
This is the transition point toward multiple scene roots: a settings or debug
scene can get its own runner context instead of sharing a process-global
`saved_view` / `saved_update` singleton, and resetting an `AppState` cannot
accidentally detach the scene root that owns it.

The frame pipeline itself is also shared. `run_scene_rebuild_frame` owns the
update, view, layout, paint, flush, trace, and scheduling-flag lifecycle, while
small backend adapters provide the native or WASI canvas operations. This keeps
Compose/SwiftUI-style scene execution as one framework contract instead of two
platform copies, and it gives future settings or debug windows a single place
to hook scene-local scheduling, diagnostics, and render-surface flushing.

Framework and example code should use the public `runtime::SceneHandle`,
`runtime::ensure_scene`, `runtime::SceneActivation`, `runtime::post`,
`runtime::post_to_scene`, and `runtime::drain_scene` APIs when it needs to
address a scene or its queue. Code that addresses drawing targets should use
the matching `runtime::RenderSurfaceHandle`, `runtime::ensure_render_surface`,
and `runtime::RenderSurfaceActivation` APIs. `detail::ScopedSceneActivation`,
`detail::ScopedRenderSurfaceActivation`, and `detail::post` remain the
low-level implementation path used by widgets, native hosts, and the runner,
but application code should not need to reach into them. This keeps the surface
area closer to SwiftUI's scene handles, React's explicit roots, and desktop
window handles in Electron/Tauri: scene identity is explicit, queue ownership
stays scene-local, render-surface ownership is explicit, and callers do not
depend on the singleton layout used by the compatibility runner.

Scene runners follow the same rule. `runtime::install_scene_runner`,
`runtime::trigger_scene_rebuild`, `runtime::clear_scene_runner`, and their
active-scene variants wrap the low-level callback/context runner slot with a
`SceneHandle`. They are intentionally small transitional hooks, but they let
settings or debug-window code attach one runner per scene root without poking
`detail::install_app_runner` directly, and targeted rebuilds restore the
caller's previous active scene after running.
`runtime::run_scene<State, Msg>` is the higher-level version of that transition:
it installs the same compatibility `run` frame pipeline into an explicit scene
instead of always binding `main`, so future settings/debug windows can mount a
declarative root while keeping their state, messages, and runner context
separate from the main window.
Native integrations should use `phenotype::native::run_scene<State, Msg>` when
a `native_host` represents a secondary window or surface. That wrapper first
binds the host's render surface to the requested scene, then installs the same
scene-local runner, so window creation code can keep native surface ownership
and declarative scene identity aligned.
Native renderer backends can now expose an optional surface activation hook.
macOS uses that hook to select a `RendererState` for the active
`NativeSurfaceDescriptor` instead of assuming one process-global Metal layer.
The current renderer entry points still look like the old single-window API, but
state selection now follows host activation and gives future settings/debug
windows a place to keep independent Metal layers, hit regions, debug captures,
and transient renderer caches.
AppKit event handling follows the same boundary: surface synchronization,
native event dispatch, and frame ticks now activate the `native_host` they are
servicing before touching shell, scene, renderer, or input state. The current
desktop runner still registers one main window, but the event-loop structure is
ready to dispatch each event to the host/window that owns it and to leave
unrelated windows' hover, focus, animation, and frame-timeline state untouched.
macOS also has an internal scene-window registry for secondary phenotype
windows. A registered AppKit scene window owns a stable `native_host`,
`NativeSurfaceDescriptor`, scene id, render-surface id, visibility flag, and
animation tick clock; the AppKit loop services those windows beside the main
window instead of treating settings/debug windows as global side effects. The
next migration step can mount the file-explorer settings UI into this registry
as a `SceneRole::Settings` scene.

## Migration Rules

- Keep `phenotype::run<State, Msg>` as the simple single-window entry point.
- Add new scene/window APIs around `SceneRuntime`; do not overload the global
  app singleton with more side channels.
- Treat view functions as pure rebuild descriptions. Native integrations should
  schedule work through scene queues rather than mutating view state directly.
- Keep high-frequency input state scene-local. Hover, pressed, scroll, and text
  caret updates must not invalidate unrelated windows.
- Keep renderer state surface-local before enabling multiple native phenotype
  windows. A second Metal layer must not reuse the first window's renderer
  singleton.
- Prefer small, testable ownership moves over one large rewrite. Each migration
  should preserve the existing examples and artifact contracts.

## Settings Window Path

The settings window should become a `SceneRole::Settings` scene. On macOS it can
still be opened from the native application menu, but the content should be a
phenotype scene backed by its own scene state and render surface. This prevents
settings interaction from stealing main-window hover/focus state and gives the
debug panel a clean way to inspect both windows.

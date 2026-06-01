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

The shared debug plane publishes the process owner in
`debug.platform_runtime.application_runtime`, then the per-root owners in
`debug.platform_runtime.scenes[]` and
`debug.platform_runtime.render_surfaces[]`. CLI, artifact, and side-panel tools
can now confirm which scene and surface are active, how many roots are visible,
which scenes have runners or scheduled ticks, and which surface owns size,
visibility, damage, and frame counters before relying on platform-specific
renderer diagnostics.
The same object reports whether process-wide debug payload, application debug,
platform capability, and platform runtime-details providers are installed. Those
providers are stored together in the `ApplicationRuntimeDiagnostics` owner
instead of being separate hidden globals. The log threshold and ring buffer
follow the same rule through `DiagnosticLogRuntime`, and the application-runtime
snapshot publishes both owner names plus the current log threshold. Future
scene/window APIs can inspect process diagnostics without accidentally treating
them as scene-local state.
The URL opener and app settings-menu entry point follow the same ownership
rule. `widget::link` dispatches through an `ApplicationRuntime` opener service
installed by the active backend, not through a raw widget-global function
pointer. macOS installs its Settings app-menu action into the same service
owner instead of keeping the callback as an AppKit-global function pointer.
Diagnostics expose `open_url_handler_installed` and
`settings_menu_handler_installed` next to the other application-runtime
services, so CLI/debug tools can tell whether link activation or the app-menu
Settings command is wired before synthesizing an interaction in a secondary
scene.
Render surfaces also own the paint/flush cache key (`last_paint_hash`) and
flush/skip counters. The legacy `AppState::last_paint_hash` remains mirrored for
older tests and callers, but invalidation flows through the active render
surface runtime first and updates that mirror as a compatibility detail.
`flush_if_changed` compares against the active surface so independent windows do
not accidentally suppress each other's first paint.

Each scene snapshot also carries a `schedule` object. It reports whether a
runner is installed, whether view-time animations, scrollbar animation,
input-motion, or debug-panel refresh are requesting future ticks, and how far
the frame trace/timeline counters have advanced. Native shells should query
these scene-schedule helpers instead of reading global flags directly, so a
future settings or debug window can request frames without waking unrelated
scenes.
The schedule clock itself is scene-local as well: `SceneRuntime` records the
last serviced scheduled tick, next due interval, and service count. Native
loops activate the host they are servicing, ask that scene whether its tick is
due, and then mark the tick on the same scene. This mirrors React roots and
Compose recomposition scopes more closely than a process-level animation clock:
an idle settings window no longer consumes the main window's debug cadence, and
a debug panel can keep its steady chart refresh without forcing unrelated
surfaces to rebuild.

Scene rebuild runners are stored as context-aware callbacks on `SceneRuntime`,
not on widget app state. The compatibility `run<State, Msg>` entry point still
installs one main-scene runner, but that runner now receives an explicit
context object containing its host, user state, view, and update closures, and
that context is owned by the scene rather than by a function-template static.
The older no-argument runner shim follows the same rule by wrapping the function
pointer in scene-owned storage instead of sharing a process-wide thunk slot.
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
The WASI canvas bridge follows the same owner shape at the platform boundary:
`WasiPaintRuntime` owns the command-buffer-backed paint host, and
`WasiLayoutRuntime` owns the text-measurement host. The shared command buffer is
still a process/host ABI detail, but the rest of the framework talks to it
through explicit runtime owners instead of raw globals. WASI runtime artifacts
publish those owner names and command-buffer capacity so browser/CLI adapters
can verify the bridge they are driving.
The desktop fallback backend follows the same rule with `StubNativeRuntime`,
which owns the stub renderer hit-region cache and publishes the owner in
runtime details. That keeps test/fallback behavior aligned with the native
backend architecture instead of leaving the lightweight backend as a hidden
exception.

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
The active message queue is not tracked by a separate process-global pointer;
it is always derived from the active `SceneRuntime`. That avoids a second
restore path during nested scene or render-surface activation and makes stale
queue ownership harder to reintroduce as more windows are added.
The active `AppState` follows the same rule. Compatibility helpers such as
`detail::g_app()` resolve through the active scene instead of a parallel active
app pointer, so the scene activation stack has one owner to restore and
secondary windows cannot accidentally keep drawing against a stale app state.
The remaining active scene/render-surface cursor is stored as one
`ActiveRuntimeBinding` value. Scene-only activation restores only the scene
cursor, while render-surface activation captures and restores the scene and
surface together. This makes nested settings/debug window work explicit: a
temporary scene build can leave the caller's drawing target alone, and a
temporary drawing-target build cannot restore only half of the active context.
Native hosts mirror that pattern with an `ActiveHostBinding` cursor. Host
activation captures and restores the previous host through one helper, and the
host-to-render-surface bridge uses the active host predicate instead of
mutating a raw host singleton directly. This keeps native event/tick handling
aligned with the scene/render-surface runtime contract.
The macOS Metal renderer mirrors the same cursor shape with
`ActiveRendererBinding`. Each native surface keeps an isolated renderer state,
and scoped renderer activation restores the previous surface state after nested
work such as secondary-window rendering or renderer shutdown.
macOS text input follows the same native-owner rule with `MacOSInputRuntime`:
the IME surface registry and caret-system override live together, so future
scene windows can attach input state by native surface without leaving a
separate process-global test or policy switch behind.
The AppKit shell keeps the foreground native window and surface together in
`ActiveAppKitBinding`. Window-level APIs such as size limits, aspect ratio,
settings-menu focus, and dock reopen focus resolve through that pair instead of
reading independent raw globals.
AppKit app-level shell state is grouped in `AppKitShellRuntime`: lifecycle
flags, close-button tracking, front-request retry counts, menu application
name, the shell delegate, and debug hot-key resources live behind one owner
instead of separate globals. That keeps process-level AppKit integration
separate from scene/window-local state while still making the app-level owner
explicit.
The Win32 shell mirrors that shape with `Win32ShellBinding`. Cursor updates,
window size limits, aspect-ratio constraints, and the Windows message loop
resolve through one captured/restored active shell binding instead of mutating a
raw process-level pointer.
The Windows D3D renderer mirrors macOS with `WindowsRendererRuntime`: the
default renderer, active renderer binding, and secondary surface registry share
one owner, so nested render-surface activation cannot restore a stale renderer
without the registry it points into.

Scene runners follow the same rule. `runtime::install_scene_runner`,
`runtime::trigger_scene_rebuild`, `runtime::clear_scene_runner`, and their
active-scene variants wrap the low-level callback/context runner slot with a
`SceneHandle`. They are intentionally small transitional hooks, but they let
settings or debug-window code attach one runner per scene root without poking
`detail::install_app_runner` directly, and targeted rebuilds restore the
caller's previous active scene after running.
View-build context follows the same boundary. Material/glass container,
transition, identity, and active-surface scopes are transient stacks on the
active scene rather than process-wide globals. Their storage is owned by
`SceneRuntime` through a type-erased extension slot, so a settings or debug
scene can build a glass button, matched-geometry transition, or material
surface while the main scene is in the middle of a rebuild without inheriting
the main scene's current material union or popping its stack. This keeps
composable helpers close to React root isolation and Compose state-holder
rules: implicit context is available during a rebuild, but the owner is still
the scene root that is actively building.
The lower-level DSL build scope is scene-owned too: `Scope::current()` and the
pending keyed-child marker live on `SceneRuntime`. A nested builder can
temporarily activate another scene without inheriting the caller's implicit
parent node, framework-local call counters, or unconsumed keyed child. That
keeps structural helpers predictable when future windows rebuild independently
or when one scene triggers another scene's view work from a shared model update.
`runtime::run_scene<State, Msg>` is the higher-level version of that transition:
it installs the same compatibility `run` frame pipeline into an explicit scene
instead of always binding `main`, so future settings/debug windows can mount a
declarative root while keeping their state, messages, and runner context
separate from the main window. `runtime::run_scene_with_state<State, Msg>` is
the state-hoisting counterpart. It keeps the scene's queue, runner, framework
local storage, and render surface isolated, but lets the caller provide a shared
application state object when a secondary scene edits the same model as the main
window. That mirrors Compose's state-hoisting rule and React's explicit roots:
ownership is visible at the call site, and scene-local transient state does not
accidentally leak across windows.
Native integrations should use `phenotype::native::run_scene<State, Msg>` when
a `native_host` represents a secondary window or surface. That wrapper first
binds the host's render surface to the requested scene, then installs the same
scene-local runner, so window creation code can keep native surface ownership
and declarative scene identity aligned.
Native renderer backends can now expose an optional surface activation hook.
macOS and Windows use that hook to select a `RendererState` for the active
`NativeSurfaceDescriptor` instead of assuming one process-global Metal or D3D
surface owner.
On macOS, `MacOSRendererRuntime` owns the default renderer, active renderer
binding, and secondary renderer registry together. That keeps the temporary
active renderer pointer tied to the registry it points into, and gives future
multi-window teardown code one explicit owner for Metal layer state.
The current renderer entry points still look like the old single-window API, but
state selection now follows host activation and gives future settings/debug
windows a place to keep independent Metal layers, hit regions, debug captures,
and transient renderer caches.
macOS text measurement keeps its Core Text font cache and registered font
aliases behind `MacOSTextRuntime`. The cache is still process-wide service
state, because registered fonts and base-face resolution are Core Text
process-scope concerns, but renderer scenes reach it through one explicit owner
instead of a raw font-cache global.
Windows text measurement now follows the same boundary with
`WindowsTextRuntime`: the DirectWrite factory, format cache, rendering
parameters, and custom font registry are process-wide text services, while
renderer surfaces continue to own their GPU text atlas upload state.
Shared decoded image pixels remain process-wide resource-cache data. On macOS,
`MacOSImageRuntime` owns the decoded-image atlas, worker queues, and repaint
target registry; Windows mirrors that split with `WindowsImageRuntime`. Each
renderer surface still tracks the image-atlas generation it has uploaded into
its own GPU texture. A secondary window can therefore reuse decoded image
metadata without inheriting the first window's Metal/D3D texture, descriptor
state, or dirty/upload cursor.
Remote-image repaint delivery follows the same split. Worker completion and
decoded pixels remain process-wide cache activity, while each attached native
surface registers its own repaint target. macOS fans image-completion repaint
requests out through that surface registry during input sync, and Windows posts
`WM_PHENOTYPE_IMAGE_READY` to every registered surface window instead of using
the current IME window as a hidden singleton. Runtime diagnostics expose the
target count so secondary-window tests can catch a regression back to one
process-wide callback.
Windows text input follows the same window-owner rule. The Win32 IME adapter
keeps a fixed registry of `ImeState` records keyed by `NativeSurfaceDescriptor`
and resolves `WM_IME_*` messages from the receiving `HWND` before touching
composition text, candidate overlays, deferred repaint flags, or the subclassed
previous window procedure. This matches the platform API shape: IME composition
messages and `HIMC` lookup are window-handle based, so a settings or debug
window must not inherit the main window's composition/candidate overlay state.
AppKit event handling follows the same boundary: surface synchronization,
native event dispatch, and frame ticks now activate the `native_host` they are
servicing before touching shell, scene, renderer, or input state. The current
desktop runner still registers one main window, but the event-loop structure is
ready to dispatch each event to the host/window that owns it and to leave
unrelated windows' hover, focus, animation, and frame-timeline state untouched.
macOS also has an internal scene-window registry for secondary phenotype
windows. `AppKitSceneWindowRuntime` owns the registry and delegate, and each
registered AppKit scene window owns a stable `native_host`,
`NativeSurfaceDescriptor`, scene id, render-surface id, visibility flag, and
animation tick clock; the AppKit loop services those windows beside the main
window instead of treating settings/debug windows as global side effects.
`phenotype::native::scene_window::show_with_state` exposes that registry as the
first public native window API for shared-state secondary scenes. The
file-explorer settings window now uses it to render a real phenotype
`SceneRole::Settings` scene in a separate AppKit window instead of mirroring the
state into one-off native controls.
The older AppKit-only preferences window remains as a compatibility path, but
its lifecycle state is grouped behind an `AppKitPreferencesRuntime` registry
instead of parallel globals for the window list, delegate, and control-tag
allocator. New settings/debug surfaces should prefer scene windows; the legacy
registry exists so old callers still have one owner to close during application
shutdown.
`phenotype::native::scene_window::write_artifact_bundle` temporarily activates a
registered scene window's host and writes the same debug artifact bundle used by
the main window. This keeps visual verification and renderer diagnostics
surface-local: a settings/debug window can prove its own chrome, clear alpha,
Metal layer opacity, and frame pixels without relying on the main scene's active
renderer.
Scene/window visibility is synchronized through the same ownership boundary.
When a native scene window opens, hides, closes, or refreshes its host surface,
the bound `RenderSurfaceRuntime.visible` value updates the corresponding
`SceneRuntime.visible` snapshot as well. Applications can also use
`runtime::set_scene_visible` and `runtime::set_render_surface_visible` for
explicit lifecycle transitions. This keeps diagnostics and CLI automation from
seeing a closed settings/debug window as a live scene, while preserving the
scene's runner, queue, and framework-local storage for a later reopen.

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
- Keep native text-input/IME state surface-local as well. AppKit calls
  `NSTextInputClient` methods on a concrete editor view, so composition text,
  selected ranges, scroll tracking, caret host views, and system insertion
  indicators should be resolved from that view/window instead of one global
  transient bucket. Win32 dispatches IME messages and `ImmGetContext` through
  the receiving `HWND`, so subclassed WndProc state, composition/candidate
  overlays, and deferred repaint flags must be keyed by that window's native
  surface.
- Keep CI coverage aligned with ownership moves. Windows-native backend,
  renderer, shell, toolchain, and native-contract test changes must run the
  real Windows native build/test path instead of only the PR fast-path stub.
- Prefer small, testable ownership moves over one large rewrite. Each migration
  should preserve the existing examples and artifact contracts.

## Settings Window Path

The file-explorer settings window now opens as a `SceneRole::Settings` scene on
macOS. It is still launched from the native application menu, but the content is
a phenotype scene backed by its own scene state and render surface. This
prevents settings interaction from stealing main-window hover/focus state and
gives the debug panel a clean way to inspect both windows.

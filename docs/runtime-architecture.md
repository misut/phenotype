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
queue, and `framework_local` storage through a `SceneRuntime`. This gives the
codebase a safe place to attach future settings windows without sharing hover,
focus, queued messages, scroll offsets, widget open/closed flags, or animation
state with the main window.

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

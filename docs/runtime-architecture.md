# Runtime Architecture

Phenotype runs a declarative component tree from an application object. An app
exposes `body(ui::Context&)`, reads local state through `ui::Context::state`,
and wires interaction through callbacks. A rebuild re-evaluates the component
tree, diffs widget-local storage by stable call-site identity, lays out nodes,
and flushes a command stream to the active platform backend.

## Application Runtime

`ui::run<App>()`, `ui::run(host, app)`, and `native::run_app<App>()` install an
app runner into the active scene. The runner owns:

- the app instance,
- a reusable `ui::Context`,
- the scene-local arena and callback tables,
- per-call-site widget storage for scroll offsets, disclosure state, and
  animations.

Callbacks mutate state handles or application fields directly. State mutation
requests another rebuild with `detail::trigger_rebuild()`, which keeps the
interaction path small and makes ownership explicit in C++.

## Scenes And Surfaces

Each scene has one descriptor, one runner, and one active render surface. Native
hosts may bind additional scenes for separate windows, but each scene keeps its
own focus, hover, callback, animation, framework-local, and scheduling data.

Render surfaces track framebuffer size, content scale, paint hashes, damage
generation, and presentation counters. Repaints can skip layout when only the
command stream needs to be flushed, while rebuilds refresh the full component
tree.

## Frame Pipeline

One rebuild frame performs these phases:

1. Prepare the active scene and surface.
2. Reset per-frame arena, callbacks, input handlers, overlays, and diagnostics.
3. Render `app.body(context)`.
4. Prune unused widget-local entries.
5. Layout the root node.
6. Paint into the command buffer.
7. Record frame timing and scheduling diagnostics.

Native shells use the same scene runner and wrap platform input in action
timing scopes. WASI uses the same rebuild pipeline with a WASI backend for
command emission.

## Widget-Local State

`framework_local<T>` stores implementation state by call-site identity and a
sibling counter. This lets widgets keep scroll offsets, animation progress, and
menu open flags without pushing those implementation details into app models.
Stable keyed views can override identity where repeated children need durable
state across ordering changes.

## Diagnostics

Runtime diagnostics expose:

- platform capabilities,
- input focus and caret snapshots,
- semantic tree snapshots,
- scene and surface descriptors,
- frame timeline samples,
- material plan and executor summaries,
- artifact bundle metadata.

The diagnostic payload is read-only and is designed for CLI verification,
native visual capture, and CI artifact checks.

## Public Entry Points

The public app entry points are:

- `ui::run<App>()` for WASI-style hosts,
- `ui::run(host, app)` for tests or custom hosts,
- `native::run_app<App>(width, height, title)` for desktop apps,
- `native::run_scene(scene, host, app)` for native scene integrations,
- `native::scene_window::show(options, app)` for secondary native windows.

All of them install an app object and a callback-based component runner.

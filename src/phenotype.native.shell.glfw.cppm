// GLFW-specific driver for phenotype's native shell. Sibling module to
// `phenotype.native.shell`, which holds the platform-agnostic dispatch logic.
//
// This module exists so that non-GLFW drivers (Android, future iOS) can
// reuse the shell's state machine while owning their own window / event
// loop. The public surface re-exports the GLFW-flavoured entry points so
// existing consumers (`phenotype.native`) keep the same call sites.
//
// Today (commit 2 of the shell-core-split PR) this module is a thin
// forwarder: it imports `phenotype.native.shell` and nothing lives here
// yet. Commit 3 moves the GLFW helpers and callbacks over; commit 4
// flips `platform_api` to pass `void*` instead of `GLFWwindow*`.

module;

export module phenotype.native.shell.glfw;

#if !defined(__wasi__) && !defined(__ANDROID__)
export import phenotype.native.shell;
#endif

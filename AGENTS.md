# phenotype Agent Guide

A C++23 UI toolkit built with `exon` and `intron`. Workspace-wide conventions (git, worktrees, language, toolchain, trust rules) live in the parent `AGENTS.md`.

## Layout

- `core/` — cross-platform primitives (`phenotype-core`), the only workspace member.
- `macos/`, `windows/` — native backends.
- `examples/`, `tools/` — sample apps and helper scripts (`tools/check-format.sh`).

## Build and Test

```sh
cd <worktree>
mise exec -- intron install
mise exec -- exon build
mise exec -- exon test
```

The same commands apply in PowerShell; a regular session is enough because `intron` resolves MSVC.

CI runs three jobs: `macos-15` native, `windows-2022` native, and `ubuntu-24.04` targeting `wasm32-wasi`. All three are supported targets — a failure on any of them is a real failure.

## Code Style

- C++23 with `import std;` instead of `#include`.
- No external libraries unless explicitly requested.
- No raw `new` / `delete`. Use `std::unique_ptr` for exclusive ownership, `std::shared_ptr` only for genuinely shared ownership, `std::weak_ptr` to break cycles.
- Raw pointers and references are non-owning. Prefer `std::string_view` / `std::span` for views and `std::optional` / `std::variant` over nullable owning pointers.
- Wrap C API handles in a move-only RAII type with the correct deleter at the boundary.

## MSVC `std::map` Landmine

As of MSVC 14.44 (VS 2022 17.14), the first `emplace` into an initially-empty `std::map` access-violates inside the tree when all of the following hold:

- the key is a user struct containing an owning string member,
- the value has a non-trivial destructor (`ComPtr`, `std::unique_ptr`, …),
- the map is a module-unit static or global, and
- the translation unit uses `import std;`.

macOS (libc++) is unaffected, and sibling shapes (plain string keys, primitive keys with RAII values, trivial values) stay green — the trigger needs the full conjunction.

- **Small caches (N ≲ 64):** use `std::vector<std::pair<K, V>>` with a linear scan and `K::operator==`. Reference fix: commit `6a0c67d`, the Windows text-format cache.
- **Larger read-heavy maps:** prefer `std::flat_map` once it is reliably available on the toolchain pinned by `mise.toml`; verify with a tiny build first.
- No `std::map` instantiation of the dangerous shape currently remains in this repo. If one reappears and Windows CI shows the same emplace-AV signature, fix the site rather than reintroducing the pattern, and consider a shared `small_map` helper in `cppx`.

## Generated Files

Never hand-edit `exon.lock` or the `CMakeLists.txt` files `exon` generates. Change the manifest and regenerate, and say so in the final report.

#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <source_location>
#include <string_view>
#include <utility>
#include <vector>
#endif

#include "phenotype/ui.hpp"

// Retained-state runtime for phenotype's component model.
//
// The value-tree ui::View (phenotype/ui.hpp) and the pure layout pass
// (phenotype/layout.hpp) stay immediate-mode: a component declares its tree in
// body(Context&) and the framework rebuilds that tree whenever state changes.
// This header adds the *retained* half — per-call-site state cells that survive
// rebuilds — without touching the View data contract or the layout algorithm.
//
// ui::State<T> / ui::Binding<T> are typed handles into a Runtime-owned store.
// A Runtime owns exactly one store and one rebuild hook, so a platform shell
// can keep an independent Runtime per surface (the Phase 1-3 per-surface state
// isolation) and never reach for a global. Components read and write state
// through Context, a thin borrow of the active Runtime.
namespace phenotype::ui {

class Runtime;

namespace detail {

// RTTI-free type identity: the address of a per-type static is unique per T, so
// a type-erased cell can be tagged and a key accidentally reused with a second
// type can be detected — all without pulling in <typeinfo>.
template <typename T> inline const void *TypeId() noexcept {
  static const char id{};
  return &id;
}

// FNV-1a hash of a user-supplied state key. A zero hash is bumped to 1 so a
// hashed key never collides with a future "no explicit key" sentinel.
inline std::uint32_t StableId(std::string_view value) noexcept {
  std::uint32_t hash = 2166136261u;
  for (char ch : value) {
    hash ^= static_cast<unsigned char>(ch);
    hash *= 16777619u;
  }
  return hash == 0u ? 1u : hash;
}

// Identity of one retained cell: the call site plus the user key. file is the
// std::source_location string-literal pointer; for a given call site every
// rebuild re-evaluates source_location::current() to the same literal, so the
// same logical state() call always maps to the same cell. (The
// retained-across-rebuild unit test guards this assumption on the pinned
// toolchain.) line/column/key separate sibling call sites.
struct LocalKey {
  const char *file = nullptr;
  std::uint_least32_t line = 0;
  std::uint_least32_t column = 0;
  std::uint32_t key = 0;

  friend bool operator==(const LocalKey &, const LocalKey &) noexcept = default;
};

// One type-erased state cell. The value lives in its own heap block behind a
// shared_ptr, so a borrowed T* into it stays valid even when the store vector
// grows. generation marks the last rebuild pass that touched the cell.
struct LocalCell {
  LocalKey key;
  const void *type = nullptr;
  std::shared_ptr<void> value;
  std::uint64_t generation = 0;
};

} // namespace detail

template <typename T> class Binding;

// Typed handle to a retained state cell. Holds a borrowed pointer to the value
// (stable across store growth) and a borrowed pointer to the owning Runtime so
// writes can request a rebuild. Cheap to copy — callbacks capture it by value.
// Mutating members are const: the handle's own pointers never change, only the
// pointee, so a State captured into a non-mutable lambda can still write.
template <typename T> class State {
public:
  State(T &value, Runtime *runtime) noexcept : value_(&value), runtime_(runtime) {}

  [[nodiscard]] const T &get() const noexcept { return *value_; }
  [[nodiscard]] T &get() noexcept { return *value_; }

  void set(T value) const;

  template <typename F>
    requires std::invocable<F &, T &>
  void mutate(F &&fn) const;

  [[nodiscard]] Binding<T> binding() const noexcept;

private:
  T *value_ = nullptr;
  Runtime *runtime_ = nullptr;
};

// Write-through handle for controls that replace the whole value (text fields).
// Binds either a State cell or a plain C++ lvalue; in the lvalue case there is
// no Runtime and writes do not auto-trigger a rebuild.
template <typename T> class Binding {
public:
  Binding() = default;
  explicit Binding(T &value, Runtime *runtime = nullptr) noexcept
      : value_(&value), runtime_(runtime) {}

  [[nodiscard]] bool valid() const noexcept { return value_ != nullptr; }
  [[nodiscard]] const T &get() const noexcept { return *value_; }
  [[nodiscard]] T &get() noexcept { return *value_; }

  void set(T value) const;

  template <typename F>
    requires std::invocable<F &, T &>
  void mutate(F &&fn) const;

private:
  T *value_ = nullptr;
  Runtime *runtime_ = nullptr;
};

// Owns one surface's retained state and the hook that asks the shell to rebuild.
// Created and held by the platform runner; pure-core code and tests drive it
// directly. Not a global — a shell keeps one Runtime per surface.
class Runtime {
public:
  // Borrow-or-create the cell for (loc, key) and stamp it with the current
  // generation. The returned reference stays valid until the cell is pruned,
  // even if the store vector reallocates, because each value is its own heap
  // block behind a shared_ptr.
  template <typename T>
  [[nodiscard]] T &Local(T initial, std::uint32_t key, std::source_location loc) {
    detail::LocalKey id{loc.file_name(), loc.line(), loc.column(), key};
    for (detail::LocalCell &cell : cells_) {
      if (cell.key == id) {
        cell.generation = generation_;
        // A key reused with a different type is a programmer error; rebuild the
        // cell from the new initial value rather than reinterpret the bytes.
        if (cell.type != detail::TypeId<T>()) {
          cell.type = detail::TypeId<T>();
          cell.value = std::make_shared<T>(std::move(initial));
        }
        return *static_cast<T *>(cell.value.get());
      }
    }
    auto value = std::make_shared<T>(std::move(initial));
    T *raw = value.get();
    cells_.push_back({id, detail::TypeId<T>(), std::move(value), generation_});
    return *raw;
  }

  // Open a rebuild pass. Cells touched (Local-accessed) during the pass are
  // stamped with this generation; Prune() then drops the cells the new tree no
  // longer references, so state for removed views does not leak.
  void BeginFrame() noexcept { ++generation_; }

  void Prune() {
    std::erase_if(cells_, [this](const detail::LocalCell &cell) {
      return cell.generation != generation_;
    });
  }

  void OnRebuild(std::function<void()> hook) { rebuild_ = std::move(hook); }

  void RequestRebuild() const {
    if (rebuild_) {
      rebuild_();
    }
  }

  [[nodiscard]] std::size_t cell_count() const noexcept { return cells_.size(); }

private:
  std::vector<detail::LocalCell> cells_;
  std::function<void()> rebuild_;
  std::uint64_t generation_ = 0;
};

// Thin borrow of the active Runtime handed to body(). state() returns a typed
// handle to per-call-site retained state; the string key disambiguates multiple
// cells declared on the same source line.
class Context {
public:
  explicit Context(Runtime &runtime) noexcept : runtime_(&runtime) {}

  template <typename T>
  [[nodiscard]] State<T> state(std::string_view key, T initial = T{},
      std::source_location loc = std::source_location::current()) const {
    return State<T>{
        runtime_->Local<T>(std::move(initial), detail::StableId(key), loc), runtime_};
  }

  void invalidate() const { runtime_->RequestRebuild(); }

private:
  Runtime *runtime_ = nullptr;
};

// --- out-of-line State/Binding members (need the complete Runtime) ----------

template <typename T> void State<T>::set(T value) const {
  *value_ = std::move(value);
  if (runtime_) {
    runtime_->RequestRebuild();
  }
}

template <typename T>
template <typename F>
  requires std::invocable<F &, T &>
void State<T>::mutate(F &&fn) const {
  std::forward<F>(fn)(*value_);
  if (runtime_) {
    runtime_->RequestRebuild();
  }
}

template <typename T> Binding<T> State<T>::binding() const noexcept {
  return Binding<T>{*value_, runtime_};
}

template <typename T> void Binding<T>::set(T value) const {
  *value_ = std::move(value);
  if (runtime_) {
    runtime_->RequestRebuild();
  }
}

template <typename T>
template <typename F>
  requires std::invocable<F &, T &>
void Binding<T>::mutate(F &&fn) const {
  std::forward<F>(fn)(*value_);
  if (runtime_) {
    runtime_->RequestRebuild();
  }
}

// A component declares its view tree in body(Context&). Plain C++ value
// semantics otherwise: it may hold members, references, or shared models that
// callbacks mutate before requesting a rebuild.
template <typename T>
concept Component = requires(T &app, Context &ctx) {
  { app.body(ctx) } -> std::same_as<View>;
};

} // namespace phenotype::ui

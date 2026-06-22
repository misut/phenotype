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

// One interpolation in flight, keyed by call site like a state cell. Holds the
// closed-form interval (from -> to over [start, start+duration]); the current
// value is computed from the clock each frame rather than stepped, so it is
// frame-rate independent (egui's ValueAnim shape). generation drives the same
// drop-when-untouched GC as LocalCell.
struct AnimCell {
  LocalKey key;
  float from = 0.0f;
  float to = 0.0f;
  double start_time = 0.0;
  double duration = 0.0;
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

  // Drive an interpolation toward `target`, returning the value for the clock
  // time `now` (seconds). On the first call, or when `target` changes mid-flight,
  // the interval is rebased from the current value so the motion is continuous
  // (no jump on retarget). Reaches `target` exactly at start + duration. Sets
  // the needs-tick flag while any animation is still in flight, so the shell can
  // schedule another frame and stop once everything has settled.
  //
  // The clock is injected (passed in), never read from a global, so the core
  // stays deterministic and testable — the platform shell supplies real time.
  [[nodiscard]] float Animate(
      float target, double now, double duration, std::uint32_t key, std::source_location loc) {
    detail::LocalKey id{loc.file_name(), loc.line(), loc.column(), key};
    detail::AnimCell *cell = nullptr;
    for (detail::AnimCell &candidate : anims_) {
      if (candidate.key == id) {
        cell = &candidate;
        break;
      }
    }
    if (cell == nullptr) {
      anims_.push_back({id, target, target, now, duration, generation_});
      return target;
    }

    cell->generation = generation_;
    float current = SampleAnim(*cell, now);
    if (cell->to != target) {
      cell->from = current;
      cell->to = target;
      cell->start_time = now;
      cell->duration = duration;
      current = cell->duration <= 0.0 ? target : current;
    }
    if (current != cell->to) {
      needs_tick_ = true;
    }
    return current;
  }

  // True when at least one animation queried this frame has not yet settled.
  // Cleared at the start of each rebuild pass (BeginFrame); the shell reads it
  // after body() to decide whether to schedule another frame.
  [[nodiscard]] bool needs_tick() const noexcept { return needs_tick_; }

  // Open a rebuild pass. Cells touched (Local-accessed) during the pass are
  // stamped with this generation; Prune() then drops the cells the new tree no
  // longer references, so state for removed views does not leak.
  void BeginFrame() noexcept {
    ++generation_;
    needs_tick_ = false;
  }

  void Prune() {
    std::erase_if(cells_, [this](const detail::LocalCell &cell) {
      return cell.generation != generation_;
    });
    std::erase_if(anims_, [this](const detail::AnimCell &cell) {
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
  [[nodiscard]] std::size_t anim_count() const noexcept { return anims_.size(); }

private:
  // Closed-form linear sample of an in-flight interval, clamped to [from, to]
  // at the interval ends. Easing is applied by the caller (a later slice) on
  // top of this linear factor.
  [[nodiscard]] static float SampleAnim(const detail::AnimCell &cell, double now) noexcept {
    if (cell.duration <= 0.0) {
      return cell.to;
    }
    double t = (now - cell.start_time) / cell.duration;
    if (t <= 0.0) {
      return cell.from;
    }
    if (t >= 1.0) {
      return cell.to;
    }
    return cell.from + (cell.to - cell.from) * static_cast<float>(t);
  }

  std::vector<detail::LocalCell> cells_;
  std::vector<detail::AnimCell> anims_;
  std::function<void()> rebuild_;
  std::uint64_t generation_ = 0;
  bool needs_tick_ = false;
};

// Thin borrow of the active Runtime handed to body(). state() returns a typed
// handle to per-call-site retained state; the string key disambiguates multiple
// cells declared on the same source line.
class Context {
public:
  // now is the frame's clock time in seconds, supplied by the shell each
  // rebuild; it defaults to 0 for tests and non-animating callers.
  explicit Context(Runtime &runtime, double now = 0.0) noexcept
      : runtime_(&runtime), now_(now) {}

  template <typename T>
  [[nodiscard]] State<T> state(std::string_view key, T initial = T{},
      std::source_location loc = std::source_location::current()) const {
    return State<T>{
        runtime_->Local<T>(std::move(initial), detail::StableId(key), loc), runtime_};
  }

  // Animate a float toward target over duration_ms, returning the value for this
  // frame. Call-site keyed like state(); pass a distinct key to animate several
  // values on one source line. Retargeting mid-flight is continuous.
  [[nodiscard]] float animate_float(float target, float duration_ms = 150.0f,
      std::string_view key = {}, std::source_location loc = std::source_location::current()) const {
    return runtime_->Animate(target, now_, static_cast<double>(duration_ms) / 1000.0,
        detail::StableId(key), loc);
  }

  // Animate a color channelwise toward target. Each channel is its own keyed
  // interval (disambiguated by a per-channel salt) so they interpolate together.
  [[nodiscard]] Color animate_color(Color target, float duration_ms = 150.0f,
      std::string_view key = {}, std::source_location loc = std::source_location::current()) const {
    double seconds = static_cast<double>(duration_ms) / 1000.0;
    std::uint32_t base = detail::StableId(key);
    return Color{
        runtime_->Animate(target.red, now_, seconds, base ^ 0x00000001u, loc),
        runtime_->Animate(target.green, now_, seconds, base ^ 0x00000002u, loc),
        runtime_->Animate(target.blue, now_, seconds, base ^ 0x00000003u, loc),
        runtime_->Animate(target.alpha, now_, seconds, base ^ 0x00000004u, loc),
    };
  }

  [[nodiscard]] double now() const noexcept { return now_; }

  void invalidate() const { runtime_->RequestRebuild(); }

private:
  Runtime *runtime_ = nullptr;
  double now_ = 0.0;
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

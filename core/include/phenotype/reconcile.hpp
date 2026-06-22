#pragma once

#ifndef PHENOTYPE_IMPORTS_STD_MODULE
#include <cstddef>
#include <cstdint>
#include <vector>
#endif

#include "phenotype/ui.hpp"

// Keyed reconciliation for the value-tree View.
//
// The component model rebuilds the whole View tree from state every frame
// (phenotype/runtime.hpp). To give list items a *stable identity* across those
// rebuilds — so animations and retained per-item state survive insertion,
// removal, and reordering — this header diffs one parent's children against the
// previous frame's children and reports which were reused, inserted, removed,
// or moved.
//
// The reuse rule is the same trivial predicate React, SwiftUI, and Flutter use:
// a child is the same logical element iff its kind AND its view_key match (a
// view_key of 0 means "no explicit key", so unkeyed children only match
// positionally — see Match). Matching is per-parent and per-level; keys are
// scoped to siblings, never matched across levels. ui::ForEach (components.hpp)
// stamps a distinct view_key on each list item, which is exactly the identity
// this pass keys off.
//
// The algorithm is the linear two-ended scan + middle key-map used by Flutter's
// updateChildren and React's reconcileChildrenArray: walk a common prefix, walk
// a common suffix, then resolve the middle through a key map. The key map is a
// flat std::vector scanned linearly rather than a std::map — the keys are the
// MSVC import-std struct-key + owning-value AV shape, and the middle is small in
// practice (see AGENTS.md std::map pitfall). Move detection uses React's greedy
// last-placed-index watermark: cheap and correct, though it flags more moves
// than a longest-increasing-subsequence pass would on full reversals.
namespace phenotype::ui {

// One reused (old, new) child pair. moved is true when the reuse requires a
// position change (the new order is not a forward run of the old order).
struct ChildMatch {
  std::size_t old_index = 0;
  std::size_t new_index = 0;
  bool moved = false;
};

// The result of reconciling one parent's children. matched is in new-child
// order; inserted / removed hold the indices into the new / old child lists
// that have no counterpart.
struct ChildDiff {
  std::vector<ChildMatch> matched;
  std::vector<std::size_t> inserted; // indices into the new children
  std::vector<std::size_t> removed;  // indices into the old children
};

namespace detail {

// Two children are the same logical element iff kind and key match. An unkeyed
// child (view_key == 0) can only match another unkeyed child of the same kind,
// which is the positional (prefix/suffix) case — keyed children match by key
// anywhere in the middle.
[[nodiscard]] inline bool Match(const View &lhs, const View &rhs) noexcept {
  return lhs.kind == rhs.kind && lhs.view_key == rhs.view_key;
}

} // namespace detail

// Reconcile old children against new children, reporting reuse / insert /
// remove / move. O(n) in the common (prefix/suffix) cases; the middle falls
// back to a linear key-map scan.
[[nodiscard]] inline ChildDiff ReconcileChildren(
    const std::vector<View> &old_children, const std::vector<View> &new_children) {
  ChildDiff diff;
  const std::size_t old_count = old_children.size();
  const std::size_t new_count = new_children.size();

  // 1. Common prefix: sync front-to-back while children match in place.
  std::size_t prefix = 0;
  while (prefix < old_count && prefix < new_count &&
         detail::Match(old_children[prefix], new_children[prefix])) {
    diff.matched.push_back({prefix, prefix, false});
    ++prefix;
  }

  // 2. Common suffix: sync back-to-front over what the prefix did not consume.
  std::size_t old_tail = old_count;
  std::size_t new_tail = new_count;
  std::vector<ChildMatch> suffix;
  while (old_tail > prefix && new_tail > prefix &&
         detail::Match(old_children[old_tail - 1], new_children[new_tail - 1])) {
    --old_tail;
    --new_tail;
    suffix.push_back({old_tail, new_tail, false});
  }

  // 3. Middle: [prefix, *_tail) on each side. Build a key map of the remaining
  //    old children, then walk the remaining new children matching against it.
  //    A greedy watermark over the matched old indices flags moves.
  struct KeyedOld {
    std::uint64_t key;
    ViewKind kind;
    std::size_t index;
    bool consumed;
  };
  std::vector<KeyedOld> remaining_old;
  remaining_old.reserve(old_tail - prefix);
  for (std::size_t index = prefix; index < old_tail; ++index) {
    remaining_old.push_back(
        {old_children[index].view_key, old_children[index].kind, index, false});
  }

  std::size_t highest_matched_old = 0;
  bool have_watermark = false;
  for (std::size_t index = prefix; index < new_tail; ++index) {
    const View &candidate = new_children[index];
    std::size_t found = remaining_old.size();
    for (std::size_t scan = 0; scan < remaining_old.size(); ++scan) {
      KeyedOld &entry = remaining_old[scan];
      if (!entry.consumed && entry.key == candidate.view_key && entry.kind == candidate.kind) {
        found = scan;
        break;
      }
    }
    if (found == remaining_old.size()) {
      diff.inserted.push_back(index);
      continue;
    }
    remaining_old[found].consumed = true;
    std::size_t old_index = remaining_old[found].index;
    // A reused child moves when it sits before the highest old index placed so
    // far (i.e. it is out of forward order relative to an already-placed match).
    bool moved = have_watermark && old_index < highest_matched_old;
    if (!moved) {
      highest_matched_old = old_index;
      have_watermark = true;
    }
    diff.matched.push_back({old_index, index, moved});
  }

  // Any remaining old middle children were not reused: they are removals.
  for (const KeyedOld &entry : remaining_old) {
    if (!entry.consumed) {
      diff.removed.push_back(entry.index);
    }
  }

  // 4. The suffix matches were collected back-to-front; append them in forward
  //    order so matched stays in new-child order overall.
  for (auto it = suffix.rbegin(); it != suffix.rend(); ++it) {
    diff.matched.push_back(*it);
  }

  return diff;
}

} // namespace phenotype::ui

import phenotype;
import std;

namespace ui = phenotype::ui;

namespace {

// A keyed text child, the shape ui::ForEach produces.
ui::View Keyed(std::uint64_t key, std::string_view content) {
  return ui::text(content).key(key);
}

// Find the match whose new_index == index, or nullptr.
const ui::ChildMatch *MatchForNew(const ui::ChildDiff &diff, std::size_t new_index) {
  for (const ui::ChildMatch &match : diff.matched) {
    if (match.new_index == new_index) {
      return &match;
    }
  }
  return nullptr;
}

} // namespace

// Covers the keyed reconciler: the reuse predicate, the prefix/suffix fast
// paths, the middle key-map, and move detection.
int main() {
  // --- Identical lists reuse everything in place, no moves -----------------
  {
    std::vector<ui::View> a;
    a.push_back(Keyed(1, "a"));
    a.push_back(Keyed(2, "b"));
    a.push_back(Keyed(3, "c"));
    std::vector<ui::View> b;
    b.push_back(Keyed(1, "a"));
    b.push_back(Keyed(2, "b"));
    b.push_back(Keyed(3, "c"));

    ui::ChildDiff diff = ui::ReconcileChildren(a, b);
    if (diff.matched.size() != 3 || !diff.inserted.empty() || !diff.removed.empty()) {
      return 1;
    }
    for (const ui::ChildMatch &match : diff.matched) {
      if (match.moved || match.old_index != match.new_index) {
        return 2;
      }
    }
  }

  // --- Append: prefix matches, the new tail is an insertion ----------------
  {
    std::vector<ui::View> a;
    a.push_back(Keyed(1, "a"));
    a.push_back(Keyed(2, "b"));
    std::vector<ui::View> b;
    b.push_back(Keyed(1, "a"));
    b.push_back(Keyed(2, "b"));
    b.push_back(Keyed(3, "c"));

    ui::ChildDiff diff = ui::ReconcileChildren(a, b);
    if (diff.matched.size() != 2 || diff.inserted.size() != 1 || !diff.removed.empty()) {
      return 3;
    }
    if (diff.inserted.front() != 2) {
      return 4;
    }
  }

  // --- Remove from the middle: prefix + suffix match, middle old is removed -
  {
    std::vector<ui::View> a;
    a.push_back(Keyed(1, "a"));
    a.push_back(Keyed(2, "b"));
    a.push_back(Keyed(3, "c"));
    std::vector<ui::View> b;
    b.push_back(Keyed(1, "a"));
    b.push_back(Keyed(3, "c"));

    ui::ChildDiff diff = ui::ReconcileChildren(a, b);
    if (diff.matched.size() != 2 || !diff.inserted.empty() || diff.removed.size() != 1) {
      return 5;
    }
    if (diff.removed.front() != 1) { // old index of key 2
      return 6;
    }
  }

  // --- Insert at the front: suffix matches, new head is an insertion --------
  {
    std::vector<ui::View> a;
    a.push_back(Keyed(2, "b"));
    a.push_back(Keyed(3, "c"));
    std::vector<ui::View> b;
    b.push_back(Keyed(1, "a"));
    b.push_back(Keyed(2, "b"));
    b.push_back(Keyed(3, "c"));

    ui::ChildDiff diff = ui::ReconcileChildren(a, b);
    if (diff.inserted.size() != 1 || diff.inserted.front() != 0 || diff.matched.size() != 2) {
      return 7;
    }
    // The reused children did not move (still a forward run).
    for (const ui::ChildMatch &match : diff.matched) {
      if (match.moved) {
        return 8;
      }
    }
  }

  // --- Reorder: a swap flags the out-of-order child as moved ---------------
  {
    std::vector<ui::View> a;
    a.push_back(Keyed(1, "a"));
    a.push_back(Keyed(2, "b"));
    a.push_back(Keyed(3, "c"));
    std::vector<ui::View> b;
    b.push_back(Keyed(3, "c"));
    b.push_back(Keyed(1, "a"));
    b.push_back(Keyed(2, "b"));

    ui::ChildDiff diff = ui::ReconcileChildren(a, b);
    // All three reused, nothing inserted/removed.
    if (diff.matched.size() != 3 || !diff.inserted.empty() || !diff.removed.empty()) {
      return 9;
    }
    // key 3 moved to the front: it is the watermark (old 2), then keys 1 and 2
    // sit before it, so exactly the later ones are flagged moved.
    std::size_t moved_count = 0;
    for (const ui::ChildMatch &match : diff.matched) {
      if (match.moved) {
        ++moved_count;
      }
    }
    if (moved_count == 0) {
      return 10;
    }
    // key 3 (new_index 0) is placed first and sets the watermark — not a move.
    const ui::ChildMatch *first = MatchForNew(diff, 0);
    if (!first || first->old_index != 2 || first->moved) {
      return 11;
    }
  }

  // --- Replace by key: same position, different key = remove + insert -------
  {
    std::vector<ui::View> a;
    a.push_back(Keyed(1, "a"));
    std::vector<ui::View> b;
    b.push_back(Keyed(2, "z"));

    ui::ChildDiff diff = ui::ReconcileChildren(a, b);
    if (diff.matched.size() != 0 || diff.inserted.size() != 1 || diff.removed.size() != 1) {
      return 12;
    }
  }

  // --- Different kind at same key does not match ---------------------------
  {
    std::vector<ui::View> a;
    a.push_back(ui::panel(ui::white()).key(7));
    std::vector<ui::View> b;
    b.push_back(ui::text("x").key(7));

    ui::ChildDiff diff = ui::ReconcileChildren(a, b);
    if (!diff.matched.empty() || diff.inserted.size() != 1 || diff.removed.size() != 1) {
      return 13;
    }
  }

  // --- Empty cases ---------------------------------------------------------
  {
    std::vector<ui::View> empty;
    std::vector<ui::View> one;
    one.push_back(Keyed(1, "a"));

    ui::ChildDiff grow = ui::ReconcileChildren(empty, one);
    if (grow.inserted.size() != 1 || !grow.matched.empty() || !grow.removed.empty()) {
      return 14;
    }
    ui::ChildDiff shrink = ui::ReconcileChildren(one, empty);
    if (shrink.removed.size() != 1 || !shrink.matched.empty() || !shrink.inserted.empty()) {
      return 15;
    }
    ui::ChildDiff nothing = ui::ReconcileChildren(empty, empty);
    if (!nothing.matched.empty() || !nothing.inserted.empty() || !nothing.removed.empty()) {
      return 16;
    }
  }

  return 0;
}

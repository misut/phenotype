import phenotype;
import std;

namespace ui = phenotype::ui;

namespace {

// A counter component: declares its tree in body() and mutates retained state
// through callbacks. Used to exercise the rebuild hook end to end.
struct CounterApp {
  int rebuilds = 0;

  ui::View body(ui::Context &cx) {
    auto count = cx.state<int>("count", 0);
    ++rebuilds;
    // The button's click handler bumps retained state; set()/mutate() then ask
    // the Runtime to rebuild. We return a plain text view of the value so the
    // produced tree is observable.
    return ui::text("count=" + std::to_string(count.get())).on_click([count] {
      count.mutate([](int &value) { ++value; });
    });
  }
};

// Reads the same logical state cell twice from one call site across two
// rebuilds; the value written between them must survive. Kept in a helper so
// both reads share a single source_location.
int &CellAcross(ui::Runtime &rt) {
  return rt.Local<int>(7, 0u, std::source_location::current());
}

} // namespace

// Covers the retained-state runtime contract: call-site identity, persistence
// across rebuilds, generation-based pruning, write-through bindings, and the
// rebuild hook that the platform shell installs.
int main() {
  // --- State persists across rebuilds, keyed by call site -----------------
  {
    ui::Runtime rt;
    rt.BeginFrame();
    int &first = CellAcross(rt);
    if (first != 7) {
      return 1;
    }
    first = 42;
    rt.Prune();

    rt.BeginFrame();
    int &second = CellAcross(rt);
    // Same call site -> same cell -> the write survives the rebuild, and the
    // borrowed reference is stable across store access.
    if (second != 42 || &first != &second) {
      return 2;
    }
    rt.Prune();
  }

  // --- Distinct keys on the same line get distinct cells ------------------
  {
    ui::Runtime rt;
    rt.BeginFrame();
    std::source_location here = std::source_location::current();
    int &a = rt.Local<int>(1, ui::detail::StableId("a"), here);
    int &b = rt.Local<int>(2, ui::detail::StableId("b"), here);
    if (&a == &b || a != 1 || b != 2) {
      return 3;
    }
    a = 100;
    // Re-fetching key "a" returns the same cell, untouched by "b".
    int &a_again = rt.Local<int>(999, ui::detail::StableId("a"), here);
    if (&a_again != &a || a_again != 100) {
      return 4;
    }
    rt.Prune();
    if (rt.cell_count() != 2) {
      return 5;
    }
  }

  // --- Pruning drops cells not touched this frame -------------------------
  {
    ui::Runtime rt;
    std::source_location here = std::source_location::current();

    rt.BeginFrame();
    rt.Local<int>(0, ui::detail::StableId("keep"), here);
    rt.Local<int>(0, ui::detail::StableId("drop"), here);
    rt.Prune();
    if (rt.cell_count() != 2) {
      return 6;
    }

    // Next frame touches only "keep"; "drop" must be pruned.
    rt.BeginFrame();
    rt.Local<int>(0, ui::detail::StableId("keep"), here);
    rt.Prune();
    if (rt.cell_count() != 1) {
      return 7;
    }
  }

  // --- Context::state + State::set fires the rebuild hook -----------------
  {
    ui::Runtime rt;
    int hook_calls = 0;
    rt.OnRebuild([&hook_calls] { ++hook_calls; });

    rt.BeginFrame();
    ui::Context cx{rt};
    auto value = cx.state<int>("v", 5);
    if (value.get() != 5) {
      return 8;
    }
    value.set(9);
    if (value.get() != 9 || hook_calls != 1) {
      return 9;
    }
    value.mutate([](int &v) { v += 1; });
    if (value.get() != 10 || hook_calls != 2) {
      return 10;
    }
    rt.Prune();
  }

  // --- Binding writes through to the same cell ----------------------------
  {
    ui::Runtime rt;
    int hook_calls = 0;
    rt.OnRebuild([&hook_calls] { ++hook_calls; });

    rt.BeginFrame();
    ui::Context cx{rt};
    auto text = cx.state<std::string>("text", std::string{"hi"});
    ui::Binding<std::string> bound = text.binding();
    if (!bound.valid() || bound.get() != "hi") {
      return 11;
    }
    bound.set("world");
    // Writing through the binding updates the underlying state cell and
    // triggers a rebuild.
    if (text.get() != "world" || hook_calls != 1) {
      return 12;
    }
    rt.Prune();
  }

  // --- Binding over a plain lvalue: no Runtime, no auto-rebuild -----------
  {
    int local = 3;
    ui::Binding<int> bound{local};
    bound.set(8);
    if (local != 8) {
      return 13;
    }
    bound.mutate([](int &v) { v *= 2; });
    if (local != 16) {
      return 14;
    }
  }

  // --- A reused key with a different type rebuilds the cell ---------------
  {
    ui::Runtime rt;
    std::source_location here = std::source_location::current();
    rt.BeginFrame();
    int &as_int = rt.Local<int>(11, ui::detail::StableId("x"), here);
    as_int = 123;
    // Same (loc,key) but a different T: must not reinterpret the int bytes.
    double &as_double = rt.Local<double>(2.5, ui::detail::StableId("x"), here);
    if (as_double != 2.5) {
      return 15;
    }
    rt.Prune();
  }

  // --- Component concept + end-to-end rebuild loop ------------------------
  {
    static_assert(ui::Component<CounterApp>);

    ui::Runtime rt;
    CounterApp app;
    // The shell's rebuild hook re-invokes body() inside a fresh frame, exactly
    // as a platform runner would on each state change.
    std::function<void()> rebuild = [&] {
      rt.BeginFrame();
      ui::Context cx{rt};
      ui::View tree = app.body(cx);
      (void)tree;
      rt.Prune();
    };
    rt.OnRebuild(rebuild);

    // Initial build.
    rebuild();
    if (app.rebuilds != 1) {
      return 16;
    }

    // Fetch the click action from a freshly built tree and fire it; the
    // handler mutates state -> RequestRebuild -> hook -> body() runs again.
    {
      rt.BeginFrame();
      ui::Context cx{rt};
      ui::View tree = app.body(cx);
      int before = app.rebuilds;
      if (!tree.click_action) {
        return 17;
      }
      tree.click_action();
      if (app.rebuilds != before + 1) {
        return 18;
      }
    }

    // State advanced: the latest tree reflects the incremented count.
    {
      rt.BeginFrame();
      ui::Context cx{rt};
      ui::View tree = app.body(cx);
      rt.Prune();
      if (tree.text_content != "count=1") {
        return 19;
      }
    }
  }

  return 0;
}

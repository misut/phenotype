#include <string>
#include <variant>
import phenotype;

// 7GUIs task #7 (Cells) — M8-1 skeleton.
//
// First step of the 7GUIs ladder (backward pass) on the native side.
// Renders an A–Z × 0–99 placeholder grid using only existing widgets.
// No state, no editing, no formulas, no dependency tracking.
//
// The current native widget set has no Box-with-border primitive, so
// data cells render as plain `·` labels rather than visibly bordered
// boxes — a fidelity gap deliberately left for M8-2 to close with a
// dedicated Cell widget.

struct State {};

struct Noop {};
using Msg = std::variant<Noop>;

void update(State&, Msg) {}

void view(State const&) {
    using namespace phenotype;
    constexpr int COLS = 26;
    constexpr int ROWS = 100;

    layout::scaffold(
        [] {
            widget::text("Cells");
            widget::text(
                "7GUIs task #7 — M8-1 skeleton. A–Z columns, 0–99 rows. "
                "Editing, formulas, dependency propagation, and viewport "
                "virtualization land in M8-2 and beyond.");
        },
        [&] {
            // Header row: blank corner + A..Z column letters.
            layout::row([&] {
                widget::text("   ");
                for (int c = 0; c < COLS; c++) {
                    widget::text(std::string(1, static_cast<char>('A' + c)));
                }
            });
            // Data rows: row number + 26 placeholder cells.
            for (int r = 0; r < ROWS; r++) {
                layout::row([&] {
                    widget::text(std::to_string(r));
                    for (int c = 0; c < COLS; c++) {
                        (void)c;
                        widget::text("·");
                    }
                });
            }
        },
        [] {
            widget::text(
                "Cell borders, selection, and editing arrive with the Cell "
                "widget in M8-2. The current native widget set has no "
                "Box-with-border primitive, so the skeleton renders as "
                "plain labeled rows.");
        }
    );
}

int main() {
    phenotype::run<State, Msg>(view, update);
    return 0;
}

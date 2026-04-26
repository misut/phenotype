#include <string>
#include <variant>
import phenotype;
import phenotype.native;

// 7GUIs task #7 (Cells) — M8-2 with the new widget::cell.
//
// Replaces the M8-1 plain-text placeholders with the new bordered
// `widget::cell`, mirroring phenotype-web's <Cell>. Each data slot is
// now a visibly bordered box; the corner / column / row labels are the
// header variant. Selection (M8-3) and edit-in-place (M8-4) are still
// pending.

struct State {};

struct Noop {};
using Msg = std::variant<Noop>;

void update(State&, Msg) {}

void view(State const&) {
    using namespace phenotype;
    constexpr int COLS = 26;
    constexpr int ROWS = 100;
    constexpr float kCellWidth = 60.0f;
    constexpr float kCellHeight = 24.0f;
    constexpr float kHeaderWidth = 36.0f;

    layout::scaffold(
        [] {
            widget::text("Cells");
            widget::text(
                "7GUIs task #7 — M8-2 with widget::cell. A–Z × 0–99 grid. "
                "Selection, editing, formulas, dependency propagation, and "
                "viewport virtualization land in M8-3 and beyond.");
        },
        [&] {
            // Header row: blank corner + A..Z column letters.
            layout::row([&] {
                widget::cell("", /*header=*/true, kHeaderWidth, kCellHeight);
                for (int c = 0; c < COLS; c++) {
                    widget::cell(std::string(1, static_cast<char>('A' + c)),
                                 /*header=*/true, kCellWidth, kCellHeight);
                }
            });
            // Data rows: row number header + 26 empty data cells.
            for (int r = 0; r < ROWS; r++) {
                layout::row([&] {
                    widget::cell(std::to_string(r), /*header=*/true,
                                 kHeaderWidth, kCellHeight);
                    for (int c = 0; c < COLS; c++) {
                        (void)c;
                        widget::cell("", /*header=*/false, kCellWidth, kCellHeight);
                    }
                });
            }
        },
        [] {
            widget::text(
                "Selection, edit-in-place, formula evaluation, dependency "
                "propagation, and viewport virtualization arrive in M8-3..M8-7.");
        }
    );
}

int main() {
    return phenotype::native::run_app<State, Msg>(800, 600, "7GUIs Cells", view, update);
}

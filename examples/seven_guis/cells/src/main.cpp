#include <string>
#include <variant>
import phenotype;
import phenotype.native;

// 7GUIs task #7 (Cells) — M8-2 with the new widget::cell.
//
// Mirrors phenotype-web/stories/SevenGuis/Cells.stories.tsx. Renders an
// A–Z × 0–99 spreadsheet skeleton with bordered cells. Selection (M8-3),
// edit-in-place (M8-4), formula evaluation (M8-5), dependency graph
// (M8-6), and viewport virtualization (M8-7) are still pending.
//
// Layout note: the React mirror lays the grid out via CSS Grid, which
// gives every column a fixed width regardless of the row's available
// width. Native phenotype currently exposes only flex layout, so the
// last cell of each row may stretch when the window is wider than the
// grid's natural width — a fidelity gap M8-N may close with either a
// `min_width` Style field or a real `layout::grid` primitive.

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

    layout::column([&] {
        widget::text("Cells", TextSize::Heading);
        widget::text(
            "7GUIs task #7 — M8-2 with widget::cell. A–Z × 0–99 grid. "
            "Selection, editing, formulas, dependency propagation, and "
            "viewport virtualization land in M8-3 and beyond.",
            TextSize::Small,
            TextColor::Muted);

        // Header row: blank corner + A..Z column letters.
        layout::row([&] {
            widget::cell("", /*header=*/true, kHeaderWidth, kCellHeight);
            for (int c = 0; c < COLS; c++) {
                widget::cell(std::string(1, static_cast<char>('A' + c)),
                             /*header=*/true, kCellWidth, kCellHeight);
            }
        }, SpaceToken::Xs);

        // Data rows: row number header + 26 empty data cells.
        for (int r = 0; r < ROWS; r++) {
            layout::row([&] {
                widget::cell(std::to_string(r), /*header=*/true,
                             kHeaderWidth, kCellHeight);
                for (int c = 0; c < COLS; c++) {
                    (void)c;
                    widget::cell("", /*header=*/false, kCellWidth, kCellHeight);
                }
            }, SpaceToken::Xs);
        }
    }, SpaceToken::Xs);
}

int main() {
    return phenotype::native::run_app<State, Msg>(800, 600, "7GUIs Cells", view, update);
}

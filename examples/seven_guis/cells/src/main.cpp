#include <string>
#include <variant>
#include <vector>
import phenotype;
import phenotype.native;

// 7GUIs task #7 (Cells) — M8-2 with the new widget::cell + layout::grid.
//
// Mirrors phenotype-web/stories/SevenGuis/Cells.stories.tsx. Renders an
// A–Z × 0–99 spreadsheet skeleton with bordered cells. Selection (M8-3),
// edit-in-place (M8-4), formula evaluation (M8-5), dependency graph
// (M8-6), and viewport virtualization (M8-7) are still pending.
//
// Layout: a single `layout::grid` with a fixed column-track template
// (36 px header + 26 × 60 px data) handles the entire spreadsheet, so
// every row sits on the same column boundaries regardless of the
// window's outer width. Borders touch (gap = 0) the way the React
// mirror's CSS Grid renders them.

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
    constexpr float kHeaderWidth = 48.0f;

    std::vector<float> cols;
    cols.reserve(COLS + 1);
    cols.push_back(kHeaderWidth);
    for (int c = 0; c < COLS; c++) cols.push_back(kCellWidth);

    layout::padded(SpaceToken::Lg, [&] { layout::column([&] {
        widget::text("Cells", TextSize::Heading);
        widget::text(
            "7GUIs task #7 — M8-2 with widget::cell + layout::grid. A–Z × 0–99 grid. "
            "Selection, editing, formulas, dependency propagation, and "
            "viewport virtualization land in M8-3 and beyond.",
            TextSize::Small,
            TextColor::Muted);

        layout::grid(std::move(cols), kCellHeight, [&] {
            // Row 0: blank corner + A..Z column letters.
            widget::cell("", /*header=*/true, kHeaderWidth, kCellHeight);
            for (int c = 0; c < COLS; c++) {
                widget::cell(std::string(1, static_cast<char>('A' + c)),
                             /*header=*/true, kCellWidth, kCellHeight);
            }
            // Rows 1..ROWS: row label + 26 empty data cells.
            for (int r = 0; r < ROWS; r++) {
                widget::cell(std::to_string(r), /*header=*/true,
                             kHeaderWidth, kCellHeight);
                for (int c = 0; c < COLS; c++) {
                    (void)c;
                    widget::cell("", /*header=*/false, kCellWidth, kCellHeight);
                }
            }
        }, /*gap=*/0.0f);
    }, SpaceToken::Sm); });
}

int main() {
    return phenotype::native::run_app<State, Msg>(900, 700, "7GUIs Cells", view, update);
}

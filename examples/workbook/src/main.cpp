#include <algorithm>
#include <array>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

import phenotype;
import phenotype.native;

namespace {

constexpr int kMonths = 12;
constexpr int kRows = 7;
constexpr int kCellCount = kMonths * kRows;

enum Row {
    Units = 0,
    AvgPrice,
    Revenue,
    Cogs,
    Opex,
    GrossProfit,
    NetMargin,
};

enum class FormatKind {
    Units,
    Currency,
    Percent,
};

struct Account {
    char const* label;
    char const* code;
    FormatKind kind;
};

constexpr std::array<char const*, kMonths> kMonthLabels = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
};

constexpr std::array<Account, kRows> kAccounts = {{
    {"Units", "units", FormatKind::Units},
    {"Avg price", "price", FormatKind::Currency},
    {"Revenue", "rev", FormatKind::Currency},
    {"COGS", "cogs", FormatKind::Currency},
    {"OpEx", "opex", FormatKind::Currency},
    {"Gross profit", "profit", FormatKind::Currency},
    {"Net margin", "margin", FormatKind::Percent},
}};

constexpr std::array<char const*, 3> kScenarioLabels = {
    "Conservative",
    "Base",
    "Upside",
};

struct ScenarioProfile {
    double start_units;
    double start_price;
    double monthly_growth;
    double price_lift;
    double cogs_rate;
    double opex_rate;
};

int cell_index(int row, int month) {
    return row * kMonths + month;
}

int cell_row(int index) {
    return index / kMonths;
}

int cell_month(int index) {
    return index % kMonths;
}

std::string address(int row, int month) {
    std::string out;
    out.push_back(static_cast<char>('A' + month));
    out += std::to_string(row + 1);
    return out;
}

std::string address(int index) {
    return address(cell_row(index), cell_month(index));
}

std::string trim(std::string_view text) {
    auto begin = text.begin();
    auto end = text.end();
    while (begin != end && std::isspace(static_cast<unsigned char>(*begin)))
        ++begin;
    while (begin != end && std::isspace(static_cast<unsigned char>(*(end - 1))))
        --end;
    return std::string(begin, end);
}

std::string fixed(double value, int precision) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(precision) << value;
    return out.str();
}

std::string percent_text(double value) {
    return fixed(value * 100.0, 1) + "%";
}

std::string raw_number(double value, int precision = 2) {
    return fixed(value, precision);
}

std::string raw_percent(double value) {
    return fixed(value * 100.0, 1) + "%";
}

std::string percent_input_value(double value) {
    return fixed(value * 100.0, 1);
}

bool parse_number_literal(std::string_view text, double& value) {
    auto cleaned = trim(text);
    if (cleaned.empty())
        return false;

    bool percent = false;
    std::string normalized;
    normalized.reserve(cleaned.size());
    for (char ch : cleaned) {
        if (ch == '$' || ch == ',')
            continue;
        if (ch == '%') {
            percent = true;
            continue;
        }
        normalized.push_back(ch);
    }

    char* end = nullptr;
    errno = 0;
    double parsed = std::strtod(normalized.c_str(), &end);
    if (end == normalized.c_str() || errno == ERANGE)
        return false;
    while (end && *end != '\0') {
        if (!std::isspace(static_cast<unsigned char>(*end)))
            return false;
        ++end;
    }

    value = percent ? parsed / 100.0 : parsed;
    return true;
}

double parse_or_default(std::string const& text, double fallback) {
    double parsed = 0.0;
    if (parse_number_literal(text, parsed))
        return parsed;
    return fallback;
}

double parse_percent_input_or_default(std::string const& text, double fallback) {
    auto cleaned = trim(text);
    auto explicit_percent = cleaned.find('%') != std::string::npos;
    double parsed = 0.0;
    if (!parse_number_literal(cleaned, parsed))
        return fallback;
    return explicit_percent ? parsed : parsed / 100.0;
}

std::string sanitize_percent_input(std::string const& text) {
    std::string out;
    bool has_dot = false;
    for (char ch : text) {
        if (std::isdigit(static_cast<unsigned char>(ch))) {
            out.push_back(ch);
        } else if (ch == '.' && !has_dot) {
            out.push_back(ch);
            has_dot = true;
        } else if ((ch == '-' || ch == '+') && out.empty()) {
            out.push_back(ch);
        }
    }
    return out;
}

ScenarioProfile scenario_profile(int scenario) {
    switch (scenario) {
    case 0:
        return {980.0, 72.0, 0.012, 0.002, 0.42, 0.29};
    case 2:
        return {1260.0, 84.0, 0.042, 0.006, 0.34, 0.20};
    default:
        return {1120.0, 78.0, 0.027, 0.004, 0.38, 0.24};
    }
}

std::vector<std::string> build_workbook_cells(
        ScenarioProfile profile,
        double monthly_growth,
        double price_lift,
        double cogs_rate,
        double opex_rate) {
    std::vector<std::string> cells(kCellCount);

    for (int month = 0; month < kMonths; ++month) {
        if (month == 0) {
            cells[cell_index(Units, month)] = raw_number(profile.start_units, 0);
            cells[cell_index(AvgPrice, month)] = raw_number(profile.start_price, 2);
        } else {
            auto prev_units = address(Units, month - 1);
            auto prev_price = address(AvgPrice, month - 1);
            cells[cell_index(Units, month)] =
                "=" + prev_units + "*" + raw_number(1.0 + monthly_growth, 4);
            cells[cell_index(AvgPrice, month)] =
                "=" + prev_price + "*" + raw_number(1.0 + price_lift, 4);
        }

        auto units = address(Units, month);
        auto price = address(AvgPrice, month);
        auto revenue = address(Revenue, month);
        auto cogs = address(Cogs, month);
        auto opex = address(Opex, month);
        auto profit = address(GrossProfit, month);

        cells[cell_index(Revenue, month)] = "=" + units + "*" + price;
        cells[cell_index(Cogs, month)] = "=" + revenue + "*" + raw_number(cogs_rate, 4);
        cells[cell_index(Opex, month)] = "=" + revenue + "*" + raw_number(opex_rate, 4);
        cells[cell_index(GrossProfit, month)] = "=" + revenue + "-" + cogs + "-" + opex;
        cells[cell_index(NetMargin, month)] = "=" + profit + "/" + revenue;
    }

    return cells;
}

struct Evaluation {
    std::vector<double> values = std::vector<double>(kCellCount, 0.0);
    std::vector<std::string> errors = std::vector<std::string>(kCellCount);
    std::vector<std::vector<int>> dependencies = std::vector<std::vector<int>>(kCellCount);
    std::vector<int> order;
    bool circular = false;
};

struct Evaluator;

struct FormulaParser {
    Evaluator& evaluator;
    int current = 0;
    std::string_view text;
    std::size_t pos = 0;

    double expression();
    double term();
    double factor();
    double number();
    double reference();
    double sum();

    void skip_ws();
    bool consume(char ch);
    bool consume_address(int& index);
    bool consume_word(std::string_view word);
    void fail(std::string message);
};

struct Evaluator {
    std::vector<std::string> const& cells;
    Evaluation result;
    std::vector<int> state = std::vector<int>(kCellCount, 0);

    explicit Evaluator(std::vector<std::string> const& raw_cells)
        : cells(raw_cells) {}

    void set_error(int index, std::string message) {
        if (result.errors[index].empty())
            result.errors[index] = std::move(message);
    }

    double eval(int index) {
        if (index < 0 || index >= kCellCount)
            return 0.0;
        if (state[index] == 2)
            return result.values[index];
        if (state[index] == 1) {
            result.circular = true;
            set_error(index, "circular reference");
            return 0.0;
        }

        state[index] = 1;
        auto raw = trim(cells[index]);

        if (raw.empty()) {
            result.values[index] = 0.0;
        } else if (raw.front() == '=') {
            auto formula = std::string_view(raw).substr(1);
            FormulaParser parser{*this, index, formula, 0};
            result.values[index] = parser.expression();
            parser.skip_ws();
            if (parser.pos != parser.text.size())
                parser.fail("unexpected token");
        } else {
            double parsed = 0.0;
            if (parse_number_literal(raw, parsed)) {
                result.values[index] = parsed;
            } else {
                set_error(index, "not a number");
                result.values[index] = 0.0;
            }
        }

        state[index] = 2;
        result.order.push_back(index);
        return result.values[index];
    }

    double dependency(int current, int target) {
        if (target < 0 || target >= kCellCount) {
            set_error(current, "bad cell reference");
            return 0.0;
        }

        auto& deps = result.dependencies[current];
        if (std::find(deps.begin(), deps.end(), target) == deps.end())
            deps.push_back(target);

        if (state[target] == 1) {
            result.circular = true;
            set_error(current, "circular reference via " + address(target));
            set_error(target, "circular reference");
            return 0.0;
        }

        double value = eval(target);
        if (!result.errors[target].empty())
            set_error(current, "depends on " + address(target));
        return value;
    }
};

void FormulaParser::skip_ws() {
    while (pos < text.size()
           && std::isspace(static_cast<unsigned char>(text[pos]))) {
        ++pos;
    }
}

bool FormulaParser::consume(char ch) {
    skip_ws();
    if (pos < text.size() && text[pos] == ch) {
        ++pos;
        return true;
    }
    return false;
}

bool FormulaParser::consume_word(std::string_view word) {
    skip_ws();
    if (pos + word.size() > text.size())
        return false;
    for (std::size_t i = 0; i < word.size(); ++i) {
        auto actual = static_cast<char>(
            std::toupper(static_cast<unsigned char>(text[pos + i])));
        auto expected = static_cast<char>(
            std::toupper(static_cast<unsigned char>(word[i])));
        if (actual != expected)
            return false;
    }
    auto next = pos + word.size();
    if (next < text.size()
        && std::isalnum(static_cast<unsigned char>(text[next]))) {
        return false;
    }
    pos = next;
    return true;
}

bool FormulaParser::consume_address(int& index) {
    skip_ws();
    if (pos >= text.size())
        return false;

    auto col = static_cast<char>(
        std::toupper(static_cast<unsigned char>(text[pos])));
    if (col < 'A' || col >= static_cast<char>('A' + kMonths))
        return false;
    ++pos;

    if (pos >= text.size()
        || !std::isdigit(static_cast<unsigned char>(text[pos]))) {
        --pos;
        return false;
    }

    int row = 0;
    while (pos < text.size()
           && std::isdigit(static_cast<unsigned char>(text[pos]))) {
        row = row * 10 + (text[pos] - '0');
        ++pos;
    }

    if (row < 1 || row > kRows) {
        fail("row out of range");
        index = -1;
        return true;
    }

    index = cell_index(row - 1, col - 'A');
    return true;
}

void FormulaParser::fail(std::string message) {
    evaluator.set_error(current, std::move(message));
    pos = text.size();
}

double FormulaParser::expression() {
    double lhs = term();
    while (true) {
        if (consume('+')) {
            lhs += term();
        } else if (consume('-')) {
            lhs -= term();
        } else {
            return lhs;
        }
    }
}

double FormulaParser::term() {
    double lhs = factor();
    while (true) {
        if (consume('*')) {
            lhs *= factor();
        } else if (consume('/')) {
            double rhs = factor();
            if (std::fabs(rhs) < 0.0000001) {
                fail("divide by zero");
                return 0.0;
            }
            lhs /= rhs;
        } else {
            return lhs;
        }
    }
}

double FormulaParser::factor() {
    skip_ws();
    if (consume('+'))
        return factor();
    if (consume('-'))
        return -factor();
    if (consume('(')) {
        double value = expression();
        if (!consume(')'))
            fail("missing ')'");
        return value;
    }
    if (consume_word("SUM"))
        return sum();

    int index = -1;
    auto saved = pos;
    if (consume_address(index)) {
        if (index < 0)
            return 0.0;
        return evaluator.dependency(current, index);
    }
    pos = saved;

    return number();
}

double FormulaParser::number() {
    skip_ws();
    std::string tail(text.substr(pos));
    char* end = nullptr;
    errno = 0;
    double parsed = std::strtod(tail.c_str(), &end);
    if (end == tail.c_str() || errno == ERANGE) {
        fail("expected number, cell, or SUM()");
        return 0.0;
    }
    pos += static_cast<std::size_t>(end - tail.c_str());
    return parsed;
}

double FormulaParser::sum() {
    if (!consume('(')) {
        fail("missing '(' after SUM");
        return 0.0;
    }

    int first = -1;
    int last = -1;
    if (!consume_address(first) || first < 0) {
        fail("bad SUM start");
        return 0.0;
    }
    if (!consume('.') || !consume('.')) {
        fail("SUM expects '..'");
        return 0.0;
    }
    if (!consume_address(last) || last < 0) {
        fail("bad SUM end");
        return 0.0;
    }
    if (!consume(')')) {
        fail("missing ')' after SUM");
        return 0.0;
    }

    int row_a = std::min(cell_row(first), cell_row(last));
    int row_b = std::max(cell_row(first), cell_row(last));
    int month_a = std::min(cell_month(first), cell_month(last));
    int month_b = std::max(cell_month(first), cell_month(last));

    double total = 0.0;
    for (int row = row_a; row <= row_b; ++row) {
        for (int month = month_a; month <= month_b; ++month)
            total += evaluator.dependency(current, cell_index(row, month));
    }
    return total;
}

Evaluation evaluate(std::vector<std::string> const& cells) {
    Evaluator evaluator(cells);
    for (int index = 0; index < kCellCount; ++index)
        (void)evaluator.eval(index);
    return std::move(evaluator.result);
}

std::string format_value(int row, double value) {
    switch (kAccounts[row].kind) {
    case FormatKind::Units:
        return fixed(value, 0);
    case FormatKind::Percent:
        return percent_text(value);
    case FormatKind::Currency:
    default:
        if (std::fabs(value) >= 1000000.0)
            return "$" + fixed(value / 1000000.0, 1) + "M";
        if (std::fabs(value) >= 1000.0)
            return "$" + fixed(value / 1000.0, 0) + "k";
        return "$" + fixed(value, 0);
    }
}

std::string formula_badge(std::string const& raw) {
    auto t = trim(raw);
    if (!t.empty() && t.front() == '=')
        return "fx";
    return "in";
}

std::string cell_label(int row, int month,
                       Evaluation const& evaluation, int selected_index) {
    auto index = cell_index(row, month);
    if (!evaluation.errors[index].empty())
        return index == selected_index ? "[ERR]" : "ERR";

    auto label = format_value(row, evaluation.values[index]);
    if (index == selected_index)
        label = "[" + label + "]";
    return label;
}

double row_sum(Evaluation const& evaluation, int row) {
    double total = 0.0;
    for (int month = 0; month < kMonths; ++month) {
        auto index = cell_index(row, month);
        if (evaluation.errors[index].empty())
            total += evaluation.values[index];
    }
    return total;
}

int error_count(Evaluation const& evaluation) {
    int count = 0;
    for (auto const& error : evaluation.errors) {
        if (!error.empty())
            ++count;
    }
    return count;
}

struct SelectCell {
    int row;
    int month;
};
struct EditSelected {
    std::string text;
};
struct SetScenario {
    int scenario;
};
struct SetGrowth {
    std::string text;
};
struct SetPriceLift {
    std::string text;
};
struct SetCogsRate {
    std::string text;
};
struct SetOpexRate {
    std::string text;
};
enum class ChartSeriesFilter {
    All,
    Revenue,
    Cost,
    Profit,
};
struct SelectChartSeries {
    ChartSeriesFilter filter;
};
struct Recalculate {};
struct ResetWorkbook {};
struct IntroduceCycle {};
struct RestoreFormulas {};

using Msg = std::variant<
    SelectCell,
    EditSelected,
    SetScenario,
    SetGrowth,
    SetPriceLift,
    SetCogsRate,
    SetOpexRate,
    SelectChartSeries,
    Recalculate,
    ResetWorkbook,
    IntroduceCycle,
    RestoreFormulas>;

Msg on_selected_edit(std::string text) {
    return EditSelected{std::move(text)};
}

Msg on_growth_edit(std::string text) {
    return SetGrowth{std::move(text)};
}

Msg on_price_lift_edit(std::string text) {
    return SetPriceLift{std::move(text)};
}

Msg on_cogs_rate_edit(std::string text) {
    return SetCogsRate{std::move(text)};
}

Msg on_opex_rate_edit(std::string text) {
    return SetOpexRate{std::move(text)};
}

void workbook_value_cell(std::string label, int row, int month) {
    using namespace phenotype;

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([msg = SelectCell{row, month}] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Button);

    auto h = detail::alloc_node();
    auto& cell_node = detail::node_at(h);
    auto const& t = detail::g_app.theme;
    cell_node.text = std::move(label);
    cell_node.font_size = t.body_font_size;
    cell_node.background = t.surface;
    cell_node.text_color = t.foreground;
    cell_node.border_color = t.border;
    cell_node.border_width = 0.5f;
    cell_node.style.padding[0] = 0;
    cell_node.style.padding[1] = t.space_sm;
    cell_node.style.padding[2] = 0;
    cell_node.style.padding[3] = t.space_xs;
    cell_node.style.max_width = 76.0f;
    cell_node.style.fixed_height = 34.0f;
    cell_node.style.text_align = TextAlign::End;
    cell_node.hover_background = t.state_hover_bg;
    cell_node.cursor_type = 1;
    cell_node.callback_id = id;
    cell_node.interaction_role = InteractionRole::Button;

    detail::attach_to_scope(h);
}

struct State {
    int scenario = 1;
    std::string growth_text;
    std::string price_lift_text;
    std::string cogs_rate_text;
    std::string opex_rate_text;
    std::vector<std::string> cells;
    std::vector<std::string> baseline;
    int selected_row = Revenue;
    int selected_month = 0;
    ChartSeriesFilter chart_filter = ChartSeriesFilter::All;
    int revision = 0;
    std::string notice = "Base forecast loaded";

    State() {
        auto profile = scenario_profile(scenario);
        growth_text = percent_input_value(profile.monthly_growth);
        price_lift_text = percent_input_value(profile.price_lift);
        cogs_rate_text = percent_input_value(profile.cogs_rate);
        opex_rate_text = percent_input_value(profile.opex_rate);
        cells = build_workbook_cells(
            profile,
            profile.monthly_growth,
            profile.price_lift,
            profile.cogs_rate,
            profile.opex_rate);
        baseline = cells;
    }
};

int selected_index(State const& state) {
    return cell_index(state.selected_row, state.selected_month);
}

char const* chart_filter_label(ChartSeriesFilter filter) {
    switch (filter) {
        case ChartSeriesFilter::Revenue: return "Revenue";
        case ChartSeriesFilter::Cost:    return "Cost";
        case ChartSeriesFilter::Profit:  return "Profit";
        case ChartSeriesFilter::All:
        default:                         return "All";
    }
}

void apply_profile(State& state, int scenario, bool reset_baseline) {
    state.scenario = scenario;
    auto profile = scenario_profile(scenario);
    state.growth_text = percent_input_value(profile.monthly_growth);
    state.price_lift_text = percent_input_value(profile.price_lift);
    state.cogs_rate_text = percent_input_value(profile.cogs_rate);
    state.opex_rate_text = percent_input_value(profile.opex_rate);
    state.cells = build_workbook_cells(
        profile,
        profile.monthly_growth,
        profile.price_lift,
        profile.cogs_rate,
        profile.opex_rate);
    if (reset_baseline)
        state.baseline = state.cells;
}

void recalculate_from_assumptions(State& state) {
    auto profile = scenario_profile(state.scenario);
    auto growth = parse_percent_input_or_default(state.growth_text, profile.monthly_growth);
    auto price_lift = parse_percent_input_or_default(state.price_lift_text, profile.price_lift);
    auto cogs_rate = parse_percent_input_or_default(state.cogs_rate_text, profile.cogs_rate);
    auto opex_rate = parse_percent_input_or_default(state.opex_rate_text, profile.opex_rate);

    state.cells = build_workbook_cells(profile, growth, price_lift, cogs_rate, opex_rate);
}

void update(State& state, Msg msg) {
    std::visit([&](auto const& event) {
        using T = std::decay_t<decltype(event)>;
        if constexpr (std::same_as<T, SelectCell>) {
            state.selected_row = event.row;
            state.selected_month = event.month;
            state.notice = "Selected " + address(event.row, event.month);
        } else if constexpr (std::same_as<T, EditSelected>) {
            state.cells[selected_index(state)] = event.text;
            state.notice = "Edited " + address(selected_index(state));
            ++state.revision;
        } else if constexpr (std::same_as<T, SetScenario>) {
            apply_profile(state, event.scenario, false);
            state.notice = std::string(kScenarioLabels[event.scenario]) + " scenario loaded";
            ++state.revision;
        } else if constexpr (std::same_as<T, SetGrowth>) {
            state.growth_text = sanitize_percent_input(event.text);
            state.notice = "Growth assumption changed";
        } else if constexpr (std::same_as<T, SetPriceLift>) {
            state.price_lift_text = sanitize_percent_input(event.text);
            state.notice = "Price lift assumption changed";
        } else if constexpr (std::same_as<T, SetCogsRate>) {
            state.cogs_rate_text = sanitize_percent_input(event.text);
            state.notice = "COGS rate assumption changed";
        } else if constexpr (std::same_as<T, SetOpexRate>) {
            state.opex_rate_text = sanitize_percent_input(event.text);
            state.notice = "OpEx rate assumption changed";
        } else if constexpr (std::same_as<T, SelectChartSeries>) {
            state.chart_filter = state.chart_filter == event.filter
                ? ChartSeriesFilter::All
                : event.filter;
            state.notice = std::string(chart_filter_label(state.chart_filter))
                + " chart series selected";
        } else if constexpr (std::same_as<T, Recalculate>) {
            recalculate_from_assumptions(state);
            state.notice = "Forecast recalculated from assumptions";
            ++state.revision;
        } else if constexpr (std::same_as<T, ResetWorkbook>) {
            state.scenario = 1;
            apply_profile(state, 1, true);
            state.selected_row = Revenue;
            state.selected_month = 0;
            state.notice = "Workbook reset to baseline";
            ++state.revision;
        } else if constexpr (std::same_as<T, IntroduceCycle>) {
            int month = state.selected_month;
            state.cells[cell_index(Revenue, month)] =
                "=" + address(GrossProfit, month) + "+1";
            state.cells[cell_index(GrossProfit, month)] =
                "=" + address(Revenue, month) + "-"
                    + address(Cogs, month) + "-" + address(Opex, month);
            state.selected_row = Revenue;
            state.notice = "Circular reference injected at " + address(Revenue, month);
            ++state.revision;
        } else if constexpr (std::same_as<T, RestoreFormulas>) {
            recalculate_from_assumptions(state);
            state.notice = "Formulas restored from current assumptions";
            ++state.revision;
        }
    }, msg);
}

std::vector<int> changed_cells(State const& state) {
    std::vector<int> changed;
    auto n = std::min(state.cells.size(), state.baseline.size());
    for (std::size_t i = 0; i < n; ++i) {
        if (state.cells[i] != state.baseline[i])
            changed.push_back(static_cast<int>(i));
    }
    return changed;
}

std::string changed_cell_text(State const& state) {
    auto changed = changed_cells(state);
    if (changed.empty())
        return "none";

    std::string out;
    int shown = 0;
    for (int index : changed) {
        if (shown > 0)
            out += ", ";
        out += address(index);
        ++shown;
        if (shown == 10 && static_cast<int>(changed.size()) > shown) {
            out += ", +" + std::to_string(static_cast<int>(changed.size()) - shown);
            break;
        }
    }
    return out;
}

std::string dependency_text(Evaluation const& evaluation, int index) {
    auto const& deps = evaluation.dependencies[index];
    if (deps.empty())
        return "none";

    std::string out;
    for (std::size_t i = 0; i < deps.size(); ++i) {
        if (i > 0)
            out += ", ";
        out += address(deps[i]);
    }
    return out;
}

std::string selected_detail(State const& state, Evaluation const& evaluation) {
    auto index = selected_index(state);
    std::string out;
    out += "cell: " + address(index);
    out += "\naccount: ";
    out += kAccounts[state.selected_row].label;
    out += "\nmonth: ";
    out += kMonthLabels[state.selected_month];
    out += "\nraw: ";
    out += state.cells[index].empty() ? "(blank)" : state.cells[index];
    out += "\nkind: ";
    out += formula_badge(state.cells[index]);
    out += "\nvalue: ";
    out += evaluation.errors[index].empty()
        ? format_value(state.selected_row, evaluation.values[index])
        : "error";
    out += "\nerror: ";
    out += evaluation.errors[index].empty() ? "none" : evaluation.errors[index];
    out += "\ndepends_on: ";
    out += dependency_text(evaluation, index);
    out += "\nrevision: ";
    out += std::to_string(state.revision);
    return out;
}

std::string error_report(Evaluation const& evaluation) {
    std::string out;
    int shown = 0;
    for (int index = 0; index < kCellCount; ++index) {
        if (evaluation.errors[index].empty())
            continue;
        if (!out.empty())
            out += "\n";
        out += address(index) + ": " + evaluation.errors[index];
        ++shown;
        if (shown == 8) {
            int remaining = error_count(evaluation) - shown;
            if (remaining > 0)
                out += "\n+" + std::to_string(remaining) + " more";
            break;
        }
    }
    if (out.empty())
        return "No formula errors.";
    return out;
}

void metric_card(phenotype::str label, std::string const& value,
                 std::string const& note) {
    using namespace phenotype;
    layout::sized_box(252.0f, [&] {
        layout::card([&] {
            widget::text(label, TextSize::Small, TextColor::Muted);
            layout::spacer(4);
            widget::text(value, TextSize::Heading);
            layout::spacer(4);
            widget::text(note, TextSize::Small, TextColor::Muted);
        });
    });
}

std::string scenario_status_note(State const& state, int errors) {
    if (errors > 0)
        return "needs review";
    if (state.notice.find("recalculated") != std::string::npos)
        return "recalculated";
    if (state.notice.find("restored") != std::string::npos)
        return "restored";
    if (state.notice.find("Edited") != std::string::npos)
        return "edited";
    if (state.notice.find("Selected") != std::string::npos)
        return "selected";
    if (state.notice.find("reset") != std::string::npos)
        return "baseline";
    return "loaded";
}

void render_summary(State const& state, Evaluation const& evaluation) {
    using namespace phenotype;

    auto revenue = row_sum(evaluation, Revenue);
    auto cost = row_sum(evaluation, Cogs) + row_sum(evaluation, Opex);
    auto profit = row_sum(evaluation, GrossProfit);
    auto margin = std::fabs(revenue) < 0.000001 ? 0.0 : profit / revenue;
    auto errors = error_count(evaluation);

    layout::row([&] {
        metric_card(
            "Scenario",
            kScenarioLabels[state.scenario],
            scenario_status_note(state, errors));
        metric_card(
            "Revenue",
            format_value(Revenue, revenue),
            "12 month total");
        metric_card(
            "Cost",
            format_value(Cogs, cost),
            "COGS + OpEx");
        metric_card(
            "Margin",
            percent_text(margin),
            errors == 0 ? "ready" : std::to_string(errors) + " formula issue(s)");
    }, SpaceToken::Sm);
}

template<typename Mapper>
void render_percent_input(phenotype::str hint,
                          std::string const& value,
                          Mapper mapper) {
    using namespace phenotype;
    layout::grid({118.0f, 32.0f}, 44.0f, [&] {
        widget::text_field<Msg>(hint, value, mapper);
        widget::cell("%", true, 32.0f, 44.0f);
    }, 0.0f);
}

void render_assumptions(State const& state) {
    using namespace phenotype;

    layout::card([&] {
        widget::text("Assumptions", TextSize::Heading);
        layout::spacer(8);
        layout::grid({190.0f, 110.0f, 130.0f}, 28.0f, [&] {
            widget::radio<Msg>("Conservative", state.scenario == 0, SetScenario{0});
            widget::radio<Msg>("Base", state.scenario == 1, SetScenario{1});
            widget::radio<Msg>("Upside", state.scenario == 2, SetScenario{2});
        }, 8.0f);
        layout::spacer(10);
        layout::grid({170.0f, 150.0f, 170.0f, 150.0f}, 44.0f, [&] {
            widget::cell("Monthly growth", true, 170.0f, 44.0f);
            render_percent_input("2.7", state.growth_text, on_growth_edit);
            widget::cell("Price lift", true, 170.0f, 44.0f);
            render_percent_input("0.4", state.price_lift_text, on_price_lift_edit);
            widget::cell("COGS rate", true, 170.0f, 44.0f);
            render_percent_input("38.0", state.cogs_rate_text, on_cogs_rate_edit);
            widget::cell("OpEx rate", true, 170.0f, 44.0f);
            render_percent_input("24.0", state.opex_rate_text, on_opex_rate_edit);
        }, 8.0f);
        layout::spacer(12);
        layout::row([&] {
            widget::button<Msg>("Recalculate forecast", Recalculate{}, ButtonVariant::Primary);
            widget::button<Msg>("Restore formulas", RestoreFormulas{});
            widget::button<Msg>("Inject circular ref", IntroduceCycle{});
            widget::button<Msg>("Reset baseline", ResetWorkbook{});
        }, SpaceToken::Sm);
    });
}

void render_workbook_grid(State const& state, Evaluation const& evaluation) {
    using namespace phenotype;
    auto selected = selected_index(state);

    layout::card([&] {
        widget::text("Workbook", TextSize::Heading);
        layout::spacer(8);

        std::vector<float> columns;
        columns.reserve(kMonths + 1);
        columns.push_back(112.0f);
        for (int month = 0; month < kMonths; ++month)
            columns.push_back(76.0f);

        layout::grid(std::move(columns), 34.0f, [&] {
            widget::cell("Account", true, 112.0f, 34.0f);
            for (int month = 0; month < kMonths; ++month)
                widget::cell(std::string(kMonthLabels[month]), true, 76.0f, 34.0f);

            for (int row = 0; row < kRows; ++row) {
                widget::cell(std::string(kAccounts[row].label), true, 112.0f, 34.0f);
                for (int month = 0; month < kMonths; ++month) {
                    auto key = static_cast<unsigned int>(cell_index(row, month) + 1);
                    phenotype::keyed(key, [&] {
                        workbook_value_cell(
                            cell_label(row, month, evaluation, selected),
                            row,
                            month);
                    });
                }
            }
        }, 0.0f);
    });
}

void render_editor(State const& state, Evaluation const& evaluation) {
    using namespace phenotype;
    auto index = selected_index(state);

    layout::card([&] {
        widget::text("Selected Cell", TextSize::Heading);
        layout::spacer(8);
        layout::row([&] {
            layout::sized_box(360.0f, [&] {
                widget::text_field<Msg>(
                    "=A1*A2 or SUM(A3..L3)",
                    state.cells[index],
                    on_selected_edit,
                    !evaluation.errors[index].empty());
            });
            widget::text(address(index), TextSize::Heading, TextColor::Accent);
            widget::text(
                std::string(kAccounts[state.selected_row].label),
                TextSize::Small,
                TextColor::Muted);
        }, SpaceToken::Md);
        layout::spacer(10);
        widget::code(selected_detail(state, evaluation));
    });
}

struct ChartSeries {
    std::array<double, kMonths> revenue{};
    std::array<double, kMonths> cost{};
    std::array<double, kMonths> profit{};
};

ChartSeries chart_series(Evaluation const& evaluation) {
    ChartSeries series;
    for (int month = 0; month < kMonths; ++month) {
        series.revenue[month] = evaluation.values[cell_index(Revenue, month)];
        series.cost[month] =
            evaluation.values[cell_index(Cogs, month)]
            + evaluation.values[cell_index(Opex, month)];
        series.profit[month] = evaluation.values[cell_index(GrossProfit, month)];
    }
    return series;
}

constexpr float kChartWidth = 980.0f;
constexpr float kChartHeight = 230.0f;
constexpr float kChartLeft = 78.0f;
constexpr float kChartTop = 20.0f;
constexpr float kChartRight = 28.0f;
constexpr float kChartBottom = 42.0f;

float chart_plot_bottom() {
    return kChartHeight - kChartBottom;
}

std::string trim_trailing_decimal(std::string text) {
    if (text.size() >= 2 && text.substr(text.size() - 2) == ".0")
        text.resize(text.size() - 2);
    return text;
}

std::string format_axis_currency(double value) {
    auto sign = value < 0 ? std::string("-") : std::string();
    auto abs_value = std::fabs(value);
    if (abs_value >= 1000000.0)
        return sign + "$" + trim_trailing_decimal(fixed(abs_value / 1000000.0, 1)) + "M";
    if (abs_value >= 1000.0)
        return sign + "$" + trim_trailing_decimal(fixed(abs_value / 1000.0, 0)) + "k";
    return sign + "$" + trim_trailing_decimal(fixed(abs_value, 0));
}

double nice_axis_ceiling(double value) {
    if (value <= 0.0)
        return 1.0;
    auto magnitude = std::pow(10.0, std::floor(std::log10(value)));
    auto normalized = value / magnitude;
    double nice = 10.0;
    if (normalized <= 1.0)
        nice = 1.0;
    else if (normalized <= 1.5)
        nice = 1.5;
    else if (normalized <= 2.0)
        nice = 2.0;
    else if (normalized <= 3.0)
        nice = 3.0;
    else if (normalized <= 5.0)
        nice = 5.0;
    return nice * magnitude;
}

float chart_x_for(int month) {
    return kChartLeft + static_cast<float>(month)
        * ((kChartWidth - kChartLeft - kChartRight) / static_cast<float>(kMonths - 1));
}

float chart_y_for(double value, double min_value, double max_value) {
    auto span = std::max(1.0, max_value - min_value);
    auto t = (value - min_value) / span;
    return static_cast<float>(
        kChartTop + (1.0 - t) * (chart_plot_bottom() - kChartTop));
}

void draw_series(phenotype::Painter& painter,
                 std::array<double, kMonths> const& values,
                 double min_value,
                 double max_value,
                 phenotype::Color color) {
    for (int month = 1; month < kMonths; ++month) {
        painter.line(
            chart_x_for(month - 1),
            chart_y_for(values[month - 1], min_value, max_value),
            chart_x_for(month),
            chart_y_for(values[month], min_value, max_value),
            2.0f,
            color);
    }
}

bool chart_filter_shows(ChartSeriesFilter active, ChartSeriesFilter series) {
    return active == ChartSeriesFilter::All || active == series;
}

void include_chart_values(std::array<double, kMonths> const& values,
                          double& min_value,
                          double& max_value) {
    for (auto value : values) {
        min_value = std::min(min_value, value);
        max_value = std::max(max_value, value);
    }
}

void draw_swatch(phenotype::Painter& painter,
                 float x,
                 float y,
                 phenotype::Color color,
                 bool filled) {
    constexpr float swatch = 10.0f;
    if (filled) {
        for (int i = 1; i < static_cast<int>(swatch); ++i) {
            auto fy = y + static_cast<float>(i);
            painter.line(x + 1.0f, fy, x + swatch - 1.0f, fy, 1.0f, color);
        }
    }
    painter.line(x, y, x + swatch, y, 2.0f, color);
    painter.line(x + swatch, y, x + swatch, y + swatch, 2.0f, color);
    painter.line(x + swatch, y + swatch, x, y + swatch, 2.0f, color);
    painter.line(x, y + swatch, x, y, 2.0f, color);
}

void render_chart_legend_item(phenotype::str label,
                              float width,
                              phenotype::Color color,
                              ChartSeriesFilter series,
                              ChartSeriesFilter active) {
    using namespace phenotype;

    auto id = static_cast<unsigned int>(detail::g_app.callbacks.size());
    detail::g_app.callbacks.push_back([msg = SelectChartSeries{series}] {
        detail::post<Msg>(msg);
        detail::trigger_rebuild();
    });
    detail::g_app.callback_roles.push_back(InteractionRole::Button);

    auto h = detail::alloc_node();
    auto& node = detail::node_at(h);
    node.style.flex_direction = FlexDirection::Row;
    node.style.max_width = width;
    node.style.fixed_height = 28.0f;
    node.callback_id = id;
    node.cursor_type = 1;
    node.focusable = false;
    node.interaction_role = InteractionRole::Button;

    bool filled = chart_filter_shows(active, series);
    detail::open_container(h, [=] {
        widget::canvas(width, 28.0f, [=](Painter& painter) {
            draw_swatch(painter, 0.0f, 13.0f, color, filled);
            painter.text(18.0f, 5.0f, label.data, label.len, 14.0f, color);
        });
    });
}

void render_chart_legend(ChartSeriesFilter active) {
    using namespace phenotype;
    Color revenue{10, 186, 181, 255};
    Color cost{220, 38, 38, 255};
    Color profit{59, 130, 246, 255};

    layout::grid({64.0f, 104.0f, 74.0f, 88.0f},
                 28.0f,
                 [&] {
        widget::canvas(64.0f, 28.0f, [](Painter&) {});
        render_chart_legend_item("Revenue", 104.0f, revenue, ChartSeriesFilter::Revenue, active);
        render_chart_legend_item("Cost", 74.0f, cost, ChartSeriesFilter::Cost, active);
        render_chart_legend_item("Profit", 88.0f, profit, ChartSeriesFilter::Profit, active);
    }, 0.0f);
}

void render_chart(Evaluation const& evaluation, ChartSeriesFilter active) {
    using namespace phenotype;
    auto series = chart_series(evaluation);

    double min_value = 0.0;
    double max_value = 1.0;
    if (chart_filter_shows(active, ChartSeriesFilter::Revenue))
        include_chart_values(series.revenue, min_value, max_value);
    if (chart_filter_shows(active, ChartSeriesFilter::Cost))
        include_chart_values(series.cost, min_value, max_value);
    if (chart_filter_shows(active, ChartSeriesFilter::Profit))
        include_chart_values(series.profit, min_value, max_value);
    auto axis_min = std::min(0.0, min_value);
    auto axis_max = nice_axis_ceiling(max_value);

    layout::card([&] {
        widget::text("Forecast Trend", TextSize::Heading);
        layout::spacer(8);
        widget::canvas(kChartWidth, kChartHeight, [series, axis_min, axis_max, active](Painter& painter) {
            Color axis{156, 163, 175, 255};
            Color grid{209, 213, 219, 255};
            Color revenue{10, 186, 181, 255};
            Color cost{220, 38, 38, 255};
            Color profit{59, 130, 246, 255};

            constexpr int y_ticks = 5;
            for (int i = 0; i < y_ticks; ++i) {
                auto t = static_cast<double>(i) / static_cast<double>(y_ticks - 1);
                auto tick = axis_max - (axis_max - axis_min) * t;
                auto y = chart_y_for(tick, axis_min, axis_max);
                auto label = format_axis_currency(tick);
                auto label_width = static_cast<float>(label.size()) * 7.0f;
                painter.text(kChartLeft - 10.0f - label_width, y - 10.0f,
                             label.c_str(), static_cast<unsigned int>(label.size()),
                             12.0f, axis);
                painter.line(kChartLeft, y, kChartWidth - kChartRight, y, 1.25f, grid);
            }
            painter.line(kChartLeft, kChartTop, kChartLeft, chart_plot_bottom(), 1.25f, grid);
            painter.line(kChartLeft, chart_plot_bottom(),
                         kChartWidth - kChartRight, chart_plot_bottom(), 1.25f, grid);
            if (chart_filter_shows(active, ChartSeriesFilter::Revenue))
                draw_series(painter, series.revenue, axis_min, axis_max, revenue);
            if (chart_filter_shows(active, ChartSeriesFilter::Cost))
                draw_series(painter, series.cost, axis_min, axis_max, cost);
            if (chart_filter_shows(active, ChartSeriesFilter::Profit))
                draw_series(painter, series.profit, axis_min, axis_max, profit);

            for (int month = 0; month < kMonths; ++month) {
                auto x = chart_x_for(month);
                painter.text(x - 9.0f, chart_plot_bottom() + 12.0f,
                             kMonthLabels[month], 3u, 12.0f, axis);
            }
        });
        render_chart_legend(active);
    });
}

void render_audit(State const& state, Evaluation const& evaluation) {
    using namespace phenotype;

    layout::card([&] {
        widget::text("Audit", TextSize::Heading);
        layout::spacer(8);
        auto changed = changed_cells(state);
        std::string audit;
        audit += "changed_cells: ";
        audit += std::to_string(static_cast<int>(changed.size()));
        audit += "\nchanged_cell_ids: ";
        audit += changed_cell_text(state);
        audit += "\nformula_errors: ";
        audit += std::to_string(error_count(evaluation));
        audit += "\ncircular_reference: ";
        audit += evaluation.circular ? "true" : "false";
        audit += "\nrecompute_order_tail: ";
        int start = std::max(0, static_cast<int>(evaluation.order.size()) - 8);
        for (int i = start; i < static_cast<int>(evaluation.order.size()); ++i) {
            if (i > start)
                audit += ", ";
            audit += address(evaluation.order[i]);
        }
        audit += "\n\n";
        audit += error_report(evaluation);
        widget::code(audit);
    });
}

void view(State const& state) {
    using namespace phenotype;
    auto evaluation = evaluate(state.cells);

    layout::padded(SpaceToken::Lg, [&] {
        layout::column([&] {
            widget::text("Revenue Forecast Workbook", TextSize::HeroTitle);
            widget::text(
                "A model-driven finance workbook with formulas, dependency recompute, validation, and a canvas trend chart.",
                TextSize::Small,
                TextColor::Muted);
            render_summary(state, evaluation);
            render_assumptions(state);
            render_workbook_grid(state, evaluation);
            render_editor(state, evaluation);
            render_chart(evaluation, state.chart_filter);
            render_audit(state, evaluation);
        }, SpaceToken::Md);
    });
}

} // namespace

int main() {
    return phenotype::native::run_app<State, Msg>(
        1180,
        820,
        "Revenue Forecast Workbook",
        view,
        update);
}

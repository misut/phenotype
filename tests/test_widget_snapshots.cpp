// Structural snapshots for the Core 6 widgets in their default state.
//
// Each test builds a single widget under a fresh arena, serialises the
// LayoutNode's visible properties (chrome + layout + interaction
// metadata) into a small JSON-like string, and compares it against an
// expected raw-string literal kept in this file.
//
// The literal *is* the snapshot: a single source of truth with no
// fixture file alongside, so updates show up as a focused diff in the
// PR. To capture a fresh snapshot, set the corresponding `expected`
// to an empty literal — assert_snapshot prints a CAPTURE block when
// the expected is empty so the workflow stays explicit.
//
// Scope: phenotype-side regression catch only. Cross-tool pixel diff
// against phenotype-web's Playwright snapshots is a separate
// milestone (font-stack + DPR + sub-pixel positioning differ).

#include <cassert>
#include <cstdint>
#include <cstdio>
#include <sstream>
#include <string>
#include <variant>

import phenotype;

using namespace phenotype;

#if defined(__wasi__)
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int /*flags*/,
                                  char const* /*family*/, unsigned int /*family_len*/,
                                  char const* /*text*/, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#endif

namespace {

std::string color_hex(Color c) {
    char buf[32];
    std::snprintf(buf, sizeof(buf), "\"#%02x%02x%02x%02x\"", c.r, c.g, c.b, c.a);
    return std::string(buf);
}

std::string flex_direction_name(FlexDirection d) {
    return d == FlexDirection::Column ? "\"column\"" : "\"row\"";
}

std::string main_align_name(MainAxisAlignment a) {
    switch (a) {
        case MainAxisAlignment::Start:        return "\"start\"";
        case MainAxisAlignment::Center:       return "\"center\"";
        case MainAxisAlignment::End:          return "\"end\"";
        case MainAxisAlignment::SpaceBetween: return "\"space-between\"";
    }
    return "\"start\"";
}

std::string cross_align_name(CrossAxisAlignment a) {
    switch (a) {
        case CrossAxisAlignment::Start:   return "\"start\"";
        case CrossAxisAlignment::Center:  return "\"center\"";
        case CrossAxisAlignment::End:     return "\"end\"";
        case CrossAxisAlignment::Stretch: return "\"stretch\"";
    }
    return "\"start\"";
}

std::string serialize_node(LayoutNode const& n) {
    std::ostringstream os;
    os << "{\n";
    os << "  \"interaction_role\": \"" << interaction_role_name(n.interaction_role) << "\",\n";
    os << "  \"flex_direction\": " << flex_direction_name(n.style.flex_direction) << ",\n";
    os << "  \"main_align\": " << main_align_name(n.style.main_align) << ",\n";
    os << "  \"cross_align\": " << cross_align_name(n.style.cross_align) << ",\n";
    os << "  \"background\": " << color_hex(n.background) << ",\n";
    os << "  \"text_color\": " << color_hex(n.text_color) << ",\n";
    os << "  \"border_color\": " << color_hex(n.border_color) << ",\n";
    os << "  \"border_width\": " << n.border_width << ",\n";
    os << "  \"border_radius\": " << n.border_radius << ",\n";
    os << "  \"padding\": [" << n.style.padding[0] << ", " << n.style.padding[1]
       << ", " << n.style.padding[2] << ", " << n.style.padding[3] << "],\n";
    os << "  \"gap\": " << n.style.gap << ",\n";
    os << "  \"font_size\": " << n.font_size << ",\n";
    os << "  \"mono\": " << (n.mono ? "true" : "false") << ",\n";
    os << "  \"cursor_type\": " << n.cursor_type << ",\n";
    os << "  \"focusable\": " << (n.focusable ? "true" : "false") << ",\n";
    os << "  \"is_input\": " << (n.is_input ? "true" : "false") << ",\n";
    os << "  \"hover_background\": " << color_hex(n.hover_background) << ",\n";
    os << "  \"hover_text_color\": " << color_hex(n.hover_text_color) << ",\n";
    os << "  \"callback_registered\": " << (n.callback_id != 0xFFFFFFFFu ? "true" : "false") << "\n";
    os << "}";
    return os.str();
}

bool assert_snapshot(char const* widget, char const* expected, std::string const& actual) {
    if (expected == nullptr || expected[0] == '\0') {
        std::printf("CAPTURE [%s]:\n%s\n\n", widget, actual.c_str());
        return false;
    }
    if (std::string(expected) != actual) {
        std::printf("MISMATCH [%s]:\n--- expected ---\n%s\n--- actual ---\n%s\n\n",
                    widget, expected, actual.c_str());
        return false;
    }
    std::printf("PASS: %s snapshot\n", widget);
    return true;
}

NodeHandle build_root() {
    detail::g_app.arena.reset();
    detail::g_app.callbacks.clear();
    detail::g_app.input_handlers.clear();
    detail::g_app.input_nodes.clear();
    detail::msg_queue().clear();
    return detail::alloc_node();
}

template<typename F>
NodeHandle build_widget(F&& fn) {
    auto root_h = build_root();
    Scope scope(root_h);
    Scope::set_current(&scope);
    fn();
    Scope::set_current(nullptr);
    auto& root = detail::node_at(root_h);
    assert(root.children.size() >= 1);
    return root.children[0];
}

struct DummyMsg {};
inline DummyMsg make_dummy(std::string) { return DummyMsg{}; }

} // namespace

bool test_text_default_snapshot() {
    auto h = build_widget([&]{ widget::text("Hello"); });
    char const* expected = R"snap({
  "interaction_role": "none",
  "flex_direction": "column",
  "main_align": "start",
  "cross_align": "start",
  "background": "#00000000",
  "text_color": "#1a1a1aff",
  "border_color": "#00000000",
  "border_width": 0,
  "border_radius": 0,
  "padding": [0, 0, 0, 0],
  "gap": 0,
  "font_size": 16,
  "mono": false,
  "cursor_type": 0,
  "focusable": true,
  "is_input": false,
  "hover_background": "#00000000",
  "hover_text_color": "#00000000",
  "callback_registered": false
})snap";
    return assert_snapshot("text-default", expected, serialize_node(detail::node_at(h)));
}

bool test_button_default_snapshot() {
    auto h = build_widget([&]{
        widget::button<DummyMsg>("Click", DummyMsg{});
    });
    char const* expected = R"snap({
  "interaction_role": "button",
  "flex_direction": "column",
  "main_align": "start",
  "cross_align": "start",
  "background": "#ffffffff",
  "text_color": "#1a1a1aff",
  "border_color": "#e5e7ebff",
  "border_width": 1,
  "border_radius": 2,
  "padding": [8, 12, 8, 12],
  "gap": 0,
  "font_size": 16,
  "mono": false,
  "cursor_type": 1,
  "focusable": true,
  "is_input": false,
  "hover_background": "#e5e7ebff",
  "hover_text_color": "#00000000",
  "callback_registered": true
})snap";
    return assert_snapshot("button-default", expected, serialize_node(detail::node_at(h)));
}

bool test_text_field_default_snapshot() {
    auto h = build_widget([&]{
        widget::text_field<DummyMsg>("Name", "", &make_dummy);
    });
    char const* expected = R"snap({
  "interaction_role": "text_field",
  "flex_direction": "column",
  "main_align": "start",
  "cross_align": "start",
  "background": "#ffffffff",
  "text_color": "#6b7280ff",
  "border_color": "#e5e7ebff",
  "border_width": 1,
  "border_radius": 2,
  "padding": [8, 12, 8, 12],
  "gap": 0,
  "font_size": 16,
  "mono": false,
  "cursor_type": 1,
  "focusable": true,
  "is_input": true,
  "hover_background": "#00000000",
  "hover_text_color": "#00000000",
  "callback_registered": true
})snap";
    return assert_snapshot("text_field-default", expected, serialize_node(detail::node_at(h)));
}

bool test_card_default_snapshot() {
    auto h = build_widget([&]{
        layout::card([&]{ widget::text("body"); });
    });
    char const* expected = R"snap({
  "interaction_role": "none",
  "flex_direction": "column",
  "main_align": "start",
  "cross_align": "start",
  "background": "#ffffffff",
  "text_color": "#1a1a1aff",
  "border_color": "#e5e7ebff",
  "border_width": 1,
  "border_radius": 4,
  "padding": [16, 16, 16, 16],
  "gap": 0,
  "font_size": 16,
  "mono": false,
  "cursor_type": 0,
  "focusable": true,
  "is_input": false,
  "hover_background": "#00000000",
  "hover_text_color": "#00000000",
  "callback_registered": false
})snap";
    return assert_snapshot("card-default", expected, serialize_node(detail::node_at(h)));
}

bool test_column_default_snapshot() {
    auto h = build_widget([&]{
        layout::column([&]{ widget::text("a"); });
    });
    char const* expected = R"snap({
  "interaction_role": "none",
  "flex_direction": "column",
  "main_align": "start",
  "cross_align": "start",
  "background": "#00000000",
  "text_color": "#1a1a1aff",
  "border_color": "#00000000",
  "border_width": 0,
  "border_radius": 0,
  "padding": [0, 0, 0, 0],
  "gap": 12,
  "font_size": 16,
  "mono": false,
  "cursor_type": 0,
  "focusable": true,
  "is_input": false,
  "hover_background": "#00000000",
  "hover_text_color": "#00000000",
  "callback_registered": false
})snap";
    return assert_snapshot("column-default", expected, serialize_node(detail::node_at(h)));
}

bool test_row_default_snapshot() {
    auto h = build_widget([&]{
        layout::row([&]{ widget::text("a"); });
    });
    char const* expected = R"snap({
  "interaction_role": "none",
  "flex_direction": "row",
  "main_align": "start",
  "cross_align": "center",
  "background": "#00000000",
  "text_color": "#1a1a1aff",
  "border_color": "#00000000",
  "border_width": 0,
  "border_radius": 0,
  "padding": [0, 0, 0, 0],
  "gap": 8,
  "font_size": 16,
  "mono": false,
  "cursor_type": 0,
  "focusable": true,
  "is_input": false,
  "hover_background": "#00000000",
  "hover_text_color": "#00000000",
  "callback_registered": false
})snap";
    return assert_snapshot("row-default", expected, serialize_node(detail::node_at(h)));
}

int main() {
    int failures = 0;
    if (!test_text_default_snapshot())       ++failures;
    if (!test_button_default_snapshot())     ++failures;
    if (!test_text_field_default_snapshot()) ++failures;
    if (!test_card_default_snapshot())       ++failures;
    if (!test_column_default_snapshot())     ++failures;
    if (!test_row_default_snapshot())        ++failures;
    if (failures > 0) {
        std::printf("\n%d snapshot(s) need updating — paste the CAPTURE block(s)"
                    " above into the corresponding `expected` literal.\n", failures);
        return 1;
    }
    std::puts("\nAll widget snapshots match.");
    return 0;
}

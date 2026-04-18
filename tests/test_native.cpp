// Native backend tests — macOS CoreText/Metal + cross-platform stub contracts.
// CoreText tests run on any macOS (no GPU needed).
// Metal tests require a Metal device (SKIP if unavailable).

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#ifndef __wasi__

#include <GLFW/glfw3.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef DrawText
#undef DrawText
#endif
#endif

import phenotype.native;
import phenotype.native.stub;
#ifdef __APPLE__
import phenotype.native.macos;
#endif

using namespace phenotype::native;
using namespace phenotype;

static void append_u32(std::vector<unsigned char>& buf, unsigned int value) {
    auto offset = buf.size();
    buf.resize(offset + 4);
    std::memcpy(buf.data() + offset, &value, 4);
}

static void append_f32(std::vector<unsigned char>& buf, float value) {
    unsigned int bits = 0;
    std::memcpy(&bits, &value, 4);
    append_u32(buf, bits);
}

static void append_bytes(std::vector<unsigned char>& buf, char const* text, unsigned int len) {
    auto offset = buf.size();
    buf.resize(offset + len);
    if (len > 0)
        std::memcpy(buf.data() + offset, text, len);
    while ((buf.size() & 3u) != 0)
        buf.push_back(0);
}

#ifdef _WIN32
struct WindowsRendererFixture {
    GLFWwindow* window = nullptr;
    native_host host{};

    WindowsRendererFixture() {
        _putenv_s("PHENOTYPE_DX12_WARP", "1");
        _putenv_s("PHENOTYPE_DX12_DEBUG", "0");

        assert(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(320, 240, "phenotype-test", nullptr, nullptr);
        assert(window != nullptr);

        text::init();
        renderer::init(window);

        host.window = window;
        host.platform = &current_platform();
    }

    ~WindowsRendererFixture() {
        renderer::shutdown();
        text::shutdown();
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
    }
};

static std::filesystem::path native_example_root() {
    return std::filesystem::path(__FILE__).parent_path().parent_path()
        / "examples" / "native";
}

static std::vector<unsigned char> make_draw_image_commands(std::string const& image_url) {
    std::vector<unsigned char> commands;
    append_u32(commands, static_cast<unsigned int>(Cmd::Clear));
    append_u32(commands, Color{245, 245, 245, 255}.packed());
    append_u32(commands, static_cast<unsigned int>(Cmd::DrawImage));
    append_f32(commands, 16.0f);
    append_f32(commands, 24.0f);
    append_f32(commands, 320.0f);
    append_f32(commands, 180.0f);
    append_u32(commands, static_cast<unsigned int>(image_url.size()));
    append_bytes(
        commands,
        image_url.data(),
        static_cast<unsigned int>(image_url.size()));
    return commands;
}

static std::string local_test_image_url() {
    auto image_path = native_example_root() / "assets" / "showcase.bmp";
    assert(std::filesystem::exists(image_path));
    return image_path.string();
}
#endif

static void test_renderer_flush_empty() {
    unsigned char buf[4] = {};
    renderer::flush(buf, 0);
    std::puts("PASS: renderer flush empty");
}

static void test_default_scroll_delta_fallback() {
    constexpr float line_height = 25.6f;
    constexpr float viewport_height = 320.0f;

    float full = phenotype::native::detail::normalize_scroll_delta(
        nullptr, 1.0, line_height, viewport_height);
    float half = phenotype::native::detail::normalize_scroll_delta(
        nullptr, 0.5, line_height, viewport_height);

    assert(std::fabs(full - line_height * 3.0f) < 0.001f);
    assert(std::fabs(half - line_height * 1.5f) < 0.001f);
    std::puts("PASS: default scroll delta fallback");
}

namespace input_regression {

struct ActivateButton {};
struct ToggleChecked {};
struct SelectChoice { int value; };
struct TextChanged { std::string text; };

using Msg = std::variant<ActivateButton, ToggleChecked, SelectChoice, TextChanged>;

struct State {
    int button_activations = 0;
    bool checked = false;
    int choice = 0;
    std::string text;
};

inline State g_observed_state{};
inline int g_link_open_count = 0;

inline constexpr unsigned int button_id = 0;
inline constexpr unsigned int link_id = 1;
inline constexpr unsigned int checkbox_id = 2;
inline constexpr unsigned int radio_a_id = 3;
inline constexpr unsigned int radio_b_id = 4;
inline constexpr unsigned int text_field_id = 5;

static void reset_core_state() {
    auto& app = phenotype::detail::g_app;
    app.app_runner = nullptr;
    app.callbacks.clear();
    app.callback_roles.clear();
    app.input_handlers.clear();
    app.input_nodes.clear();
    app.focusable_ids.clear();
    app.root = NodeHandle::null();
    app.prev_root = NodeHandle::null();
    app.scroll_y = 0.0f;
    app.hovered_id = phenotype::native::invalid_callback_id;
    app.focused_id = phenotype::native::invalid_callback_id;
    app.caret_pos = phenotype::native::invalid_callback_id;
    app.caret_visible = true;
    app.last_paint_hash = 0;
    app.input_debug = {};
    app.arena.reset();
    app.prev_arena.reset();
    phenotype::detail::msg_queue().clear();
    phenotype::metrics::reset_all();
    g_observed_state = {};
    g_link_open_count = 0;
}

static void open_url(char const*, unsigned int) {
    ++g_link_open_count;
}

static Msg map_text(std::string value) {
    return TextChanged{std::move(value)};
}

static void update(State& state, Msg msg) {
    if (std::get_if<ActivateButton>(&msg)) {
        state.button_activations += 1;
    } else if (std::get_if<ToggleChecked>(&msg)) {
        state.checked = !state.checked;
    } else if (auto const* choice = std::get_if<SelectChoice>(&msg)) {
        state.choice = choice->value;
    } else if (auto const* text = std::get_if<TextChanged>(&msg)) {
        state.text = text->text;
    }
    g_observed_state = state;
}

static void view(State const& state) {
    phenotype::layout::column([&] {
        phenotype::widget::button<Msg>("Action", ActivateButton{});
        phenotype::layout::spacer(10);
        phenotype::widget::link("Open docs", "https://example.com/phenotype");
        phenotype::layout::spacer(10);
        phenotype::widget::checkbox<Msg>("Enable option", state.checked, ToggleChecked{});
        phenotype::layout::spacer(10);
        phenotype::widget::radio<Msg>("Choice A", state.choice == 0, SelectChoice{0});
        phenotype::layout::spacer(6);
        phenotype::widget::radio<Msg>("Choice B", state.choice == 1, SelectChoice{1});
        phenotype::layout::spacer(10);
        phenotype::layout::card([&] {
            phenotype::widget::text("Nested input");
            phenotype::widget::text_field<Msg>("Type here", state.text, +map_text);
        });
        phenotype::layout::spacer(1200);
        phenotype::widget::text("Bottom marker");
    });
}

static bool has_metric(std::string_view event,
                       std::string_view detail,
                       std::string_view result,
                       std::string_view role = {}) {
    for (auto const& point : phenotype::metrics::inst::input_events.data_points()) {
        std::string_view event_attr;
        std::string_view detail_attr;
        std::string_view result_attr;
        std::string_view role_attr;
        for (auto const& attr : point.attributes) {
            if (attr.key == "event") event_attr = attr.value;
            else if (attr.key == "detail") detail_attr = attr.value;
            else if (attr.key == "result") result_attr = attr.value;
            else if (attr.key == "role") role_attr = attr.value;
        }
        if (event_attr == event
            && detail_attr == detail
            && result_attr == result
            && (role.empty() || role_attr == role))
            return true;
    }
    return false;
}

struct Harness {
    platform_api platform = make_stub_platform("test-stub", nullptr);
    native_host host{};

    Harness() {
        reset_core_state();
        platform.open_url = open_url;
        host.platform = &platform;
        phenotype::native::run<State, Msg>(host, view, update);
        assert(phenotype::detail::g_app.callbacks.size() == 6);
        assert(phenotype::detail::g_app.focusable_ids.size() == 6);
    }

    ~Harness() {
        phenotype::native::detail::shutdown_host(host);
        reset_core_state();
    }

    std::pair<float, float> point_for(unsigned int callback_id) const {
        for (float y = 0.0f; y <= 640.0f; y += 2.0f) {
            for (float x = 0.0f; x <= 360.0f; x += 2.0f) {
                auto hit = phenotype::native::detail::hit_test(
                    x, y, phenotype::detail::get_scroll_y());
                if (hit.has_value() && *hit == callback_id)
                    return {x, y};
            }
        }
        assert(false && "missing hit-test point");
        return {0.0f, 0.0f};
    }
};

} // namespace input_regression

namespace {
int g_platform_sync_calls = 0;

void count_platform_sync() {
    ++g_platform_sync_calls;
}
}

static void test_shell_pointer_hover_click_and_tab_navigation() {
    using namespace input_regression;

    Harness harness;
    auto [x, y] = harness.point_for(button_id);

    assert(phenotype::native::detail::dispatch_cursor_pos(x, y));
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.event == "hover");
    assert(debug.detail == "pointer-move");
    assert(debug.result == "handled");
    assert(debug.callback_id == button_id);
    assert(debug.role == "button");
    assert(debug.hovered_id == button_id);
    assert(has_metric("hover", "pointer-move", "handled", "button"));

    assert(phenotype::native::detail::dispatch_mouse_button(
        x, y, GLFW_MOUSE_BUTTON_LEFT, GLFW_PRESS, 0));
    assert(g_observed_state.button_activations == 1);
    assert(phenotype::detail::get_focused_id() == button_id);
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.event == "click");
    assert(debug.detail == "pointer-click");
    assert(debug.result == "handled");
    assert(debug.callback_id == button_id);
    assert(debug.focused_id == button_id);
    assert(debug.focused_role == "button");
    assert(has_metric("click", "pointer-click", "handled", "button"));

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_TAB, GLFW_PRESS, 0));
    assert(phenotype::detail::get_focused_id() == link_id);
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_TAB, GLFW_PRESS, 0));
    assert(phenotype::detail::get_focused_id() == checkbox_id);
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_TAB, GLFW_PRESS, GLFW_MOD_SHIFT));
    assert(phenotype::detail::get_focused_id() == link_id);
    std::puts("PASS: shared shell pointer hover/click and tab navigation");
}

static void test_shell_activation_keys_respect_roles() {
    using namespace input_regression;

    Harness harness;

    phenotype::detail::set_focus_id(button_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_SPACE, GLFW_PRESS, 0));
    assert(g_observed_state.button_activations == 1);

    phenotype::detail::set_focus_id(checkbox_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_SPACE, GLFW_PRESS, 0));
    assert(g_observed_state.checked);

    phenotype::detail::set_focus_id(radio_b_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_SPACE, GLFW_PRESS, 0));
    assert(g_observed_state.choice == 1);

    phenotype::detail::set_focus_id(link_id, "test", "setup");
    assert(!phenotype::native::detail::dispatch_key(GLFW_KEY_SPACE, GLFW_PRESS, 0));
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.event == "key");
    assert(debug.detail == "space");
    assert(debug.result == "ignored");
    assert(debug.role == "link");
    assert(has_metric("key", "space", "ignored", "link"));

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_ENTER, GLFW_PRESS, 0));
    assert(g_link_open_count == 1);

    phenotype::detail::set_focus_id(button_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_ENTER, GLFW_PRESS, 0));
    assert(g_observed_state.button_activations == 2);

    phenotype::detail::set_focus_id(checkbox_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_ENTER, GLFW_PRESS, 0));
    assert(!g_observed_state.checked);

    phenotype::detail::set_focus_id(radio_b_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_ENTER, GLFW_PRESS, 0));
    assert(g_observed_state.choice == 1);
    std::puts("PASS: shared shell activation keys respect roles");
}

static void test_shell_text_space_char_and_enter_behavior() {
    using namespace input_regression;

    Harness harness;

    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    assert(!phenotype::native::detail::dispatch_key(GLFW_KEY_ENTER, GLFW_PRESS, 0));
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.event == "key");
    assert(debug.detail == "enter");
    assert(debug.result == "ignored");
    assert(debug.role == "text_field");

    assert(!phenotype::native::detail::dispatch_key(GLFW_KEY_SPACE, GLFW_PRESS, 0));
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.detail == "space");
    assert(debug.result == "ignored");
    assert(g_observed_state.text.empty());
    assert(has_metric("key", "space", "ignored", "text_field"));

    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>(' ')));
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.detail == "space-char");
    assert(debug.result == "handled");
    assert(debug.caret_pos == 1);
    assert(debug.caret_visible);
    assert(g_observed_state.text == " ");
    assert(has_metric("key", "space-char", "handled", "text_field"));
    std::puts("PASS: shared shell text space char and enter behavior");
}

static void test_shell_text_caret_navigation_and_backspace() {
    using namespace input_regression;

    Harness harness;

    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('A')));
    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('B')));
    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('C')));
    assert(g_observed_state.text == "ABC");
    assert(phenotype::detail::get_caret_pos() == 3);

    phenotype::detail::toggle_caret();
    assert(!phenotype::detail::get_caret_visible());
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_LEFT, GLFW_PRESS, 0));
    assert(phenotype::detail::get_caret_pos() == 2);
    assert(phenotype::detail::get_caret_visible());
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_RIGHT, GLFW_PRESS, 0));
    assert(phenotype::detail::get_caret_pos() == 3);
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_HOME, GLFW_PRESS, 0));
    assert(phenotype::detail::get_caret_pos() == 0);
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_RIGHT, GLFW_PRESS, 0));
    assert(phenotype::detail::get_caret_pos() == 1);

    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('Z')));
    assert(g_observed_state.text == "AZBC");
    assert(phenotype::detail::get_caret_pos() == 2);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_END, GLFW_PRESS, 0));
    assert(phenotype::detail::get_caret_pos() == 4);
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_LEFT, GLFW_PRESS, 0));
    assert(phenotype::detail::get_caret_pos() == 3);
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0));
    assert(g_observed_state.text == "AZC");
    assert(phenotype::detail::get_caret_pos() == 2);

    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.event == "key");
    assert(debug.detail == "backspace");
    assert(debug.result == "handled");
    assert(debug.focused_role == "text_field");
    assert(debug.caret_pos == 2);
    assert(debug.caret_visible);
    assert(has_metric("key", "backspace", "handled", "text_field"));
    std::puts("PASS: shared shell text caret navigation and backspace");
}

static void test_shell_pointer_text_caret_placement_and_visibility_reset() {
    using namespace input_regression;

    Harness harness;

    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    assert(phenotype::detail::replace_focused_input_text(0, 0, "ABC"));

    auto click_text_field = [&](float x) {
        auto snapshot = phenotype::detail::focused_input_snapshot();
        assert(snapshot.valid);
        float y = snapshot.y + snapshot.height * 0.5f;
        return phenotype::native::detail::dispatch_mouse_button(
            x,
            y,
            GLFW_MOUSE_BUTTON_LEFT,
            GLFW_PRESS,
            0);
    };

    auto prefix_width = [&](std::string const& value, std::size_t bytes) {
        auto snapshot = phenotype::detail::focused_input_snapshot();
        return harness.host.measure_text(
            snapshot.font_size,
            snapshot.mono ? 1u : 0u,
            value.data(),
            static_cast<unsigned int>(bytes));
    };

    {
        auto snapshot = phenotype::detail::focused_input_snapshot();
        phenotype::detail::toggle_caret();
        assert(!phenotype::detail::get_caret_visible());
        assert(click_text_field(snapshot.x + 1.0f));
        assert(phenotype::detail::get_caret_pos() == 0);
        assert(phenotype::detail::get_caret_visible());
    }

    {
        auto snapshot = phenotype::detail::focused_input_snapshot();
        float base_x = snapshot.x + snapshot.padding[3];
        float x = base_x + prefix_width(snapshot.value, 1) + 1.0f;
        assert(click_text_field(x));
        assert(phenotype::detail::get_caret_pos() == 1);
        assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('Z')));
        assert(g_observed_state.text == "AZBC");
    }

    assert(phenotype::detail::replace_focused_input_text(0, g_observed_state.text.size(), "A가C"));
    {
        auto snapshot = phenotype::detail::focused_input_snapshot();
        float base_x = snapshot.x + snapshot.padding[3];
        float after_multibyte = prefix_width(snapshot.value, 4);
        phenotype::detail::toggle_caret();
        assert(!phenotype::detail::get_caret_visible());
        assert(click_text_field(base_x + after_multibyte - 1.0f));
        assert(phenotype::detail::get_caret_pos() == 4);
        assert(phenotype::detail::get_caret_visible());
    }

    {
        auto snapshot = phenotype::detail::focused_input_snapshot();
        assert(click_text_field(snapshot.x + snapshot.width - 1.0f));
        assert(phenotype::detail::get_caret_pos() == snapshot.value.size());
    }

    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.event == "click");
    assert(debug.detail == "pointer-click");
    assert(debug.result == "handled");
    assert(debug.focused_role == "text_field");
    assert(debug.caret_pos == static_cast<unsigned int>(g_observed_state.text.size()));
    assert(debug.caret_visible);
    std::puts("PASS: shared shell pointer text caret placement and visibility reset");
}

static void test_shared_caret_debug_rect_tracks_layout() {
    using namespace input_regression;

    Harness harness;
    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    assert(phenotype::detail::replace_focused_input_text(0, 0, "A가C"));
    phenotype::native::detail::repaint_current();

    auto snapshot = phenotype::detail::focused_input_snapshot();
    assert(snapshot.valid);
    float base_x = snapshot.x + snapshot.padding[3];
    auto prefix_width = [&](std::size_t bytes) {
        return harness.host.measure_text(
            snapshot.font_size,
            snapshot.mono ? 1u : 0u,
            snapshot.value.data(),
            static_cast<unsigned int>(bytes));
    };

    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.caret_renderer == "custom");
    assert(debug.caret_rect.valid);
    assert(std::fabs(debug.caret_rect.x - (base_x + prefix_width(snapshot.value.size()))) < 0.75f);
    assert(std::fabs(debug.caret_rect.y - (snapshot.y + snapshot.padding[0])) < 0.75f);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_HOME, GLFW_PRESS, 0));
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.caret_renderer == "custom");
    assert(debug.caret_rect.valid);
    assert(std::fabs(debug.caret_rect.x - base_x) < 0.75f);
    assert(std::fabs(debug.caret_rect.y - (snapshot.y + snapshot.padding[0])) < 0.75f);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_RIGHT, GLFW_PRESS, 0));
    debug = phenotype::diag::input_debug_snapshot();
    assert(std::fabs(debug.caret_rect.x - (base_x + prefix_width(1))) < 0.75f);
    assert(std::fabs(debug.caret_rect.y - (snapshot.y + snapshot.padding[0])) < 0.75f);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_RIGHT, GLFW_PRESS, 0));
    debug = phenotype::diag::input_debug_snapshot();
    assert(std::fabs(debug.caret_rect.x - (base_x + prefix_width(4))) < 0.75f);
    assert(std::fabs(debug.caret_rect.y - (snapshot.y + snapshot.padding[0])) < 0.75f);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_END, GLFW_PRESS, 0));
    debug = phenotype::diag::input_debug_snapshot();
    assert(std::fabs(debug.caret_rect.x - (base_x + prefix_width(snapshot.value.size()))) < 0.75f);
    assert(std::fabs(debug.caret_rect.y - (snapshot.y + snapshot.padding[0])) < 0.75f);

    std::puts("PASS: shared caret debug rect tracks layout");
}

static void test_caret_overlay_state_invalidates_cached_frame_hash() {
    using namespace input_regression;

    Harness harness;

    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    phenotype::native::detail::repaint_current();
    auto baseline_hash = phenotype::detail::g_app.last_paint_hash;
    assert(baseline_hash != 0);

    phenotype::detail::toggle_caret();
    assert(phenotype::detail::g_app.last_paint_hash == 0);
    phenotype::native::detail::repaint_current();
    assert(phenotype::detail::g_app.last_paint_hash != 0);

    assert(phenotype::detail::set_focused_input_caret_pos(0));
    assert(phenotype::detail::g_app.last_paint_hash == 0);

    phenotype::detail::set_input_composition_state(true, "가", 3);
    assert(phenotype::detail::g_app.last_paint_hash == 0);

    std::puts("PASS: caret overlay state invalidates cached frame hash");
}

static void test_shared_text_replacement_helper() {
    using namespace input_regression;

    Harness harness;
    phenotype::detail::set_focus_id(text_field_id, "test", "setup");

    assert(phenotype::detail::replace_focused_input_text(0, 0, "AB"));
    assert(g_observed_state.text == "AB");
    assert(phenotype::detail::get_caret_pos() == 2);

    assert(phenotype::detail::replace_focused_input_text(1, 1, "찬"));
    assert(g_observed_state.text == std::string("A") + "찬" + "B");
    assert(phenotype::detail::get_caret_pos() == 1 + std::strlen("찬"));

    assert(phenotype::detail::replace_focused_input_text(1, 4, "Z"));
    assert(g_observed_state.text == std::string("AZ") + "B");
    assert(phenotype::detail::get_caret_pos() == 2);

    assert(phenotype::detail::replace_focused_input_text(0, 0, "가"));
    assert(g_observed_state.text == std::string("가") + "AZB");
    assert(phenotype::detail::replace_focused_input_text(1, 2, "X"));
    assert(g_observed_state.text == std::string("X") + "가AZB");
    assert(phenotype::detail::get_caret_pos() == 1);

    phenotype::detail::set_input_composition_state(true, "찮", 3);
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.composition_active);
    assert(debug.composition_text == "찮");
    assert(debug.composition_cursor == 3);

    harness.platform.input.dismiss_transient = []() {
        phenotype::detail::clear_input_composition_state();
        return true;
    };
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_ESCAPE, GLFW_PRESS, 0));
    debug = phenotype::diag::input_debug_snapshot();
    assert(!debug.composition_active);
    assert(debug.composition_text.empty());
    assert(debug.composition_cursor == 0);

    std::puts("PASS: shared text replacement helper");
}

static void test_focus_transitions_sync_platform_input() {
    using namespace input_regression;

    Harness harness;
    g_platform_sync_calls = 0;
    harness.platform.input.sync = count_platform_sync;

    auto [text_x, text_y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        text_x,
        text_y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));
    auto after_click = g_platform_sync_calls;
    assert(after_click >= 1);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_TAB, GLFW_PRESS, 0));
    auto after_tab = g_platform_sync_calls;
    assert(after_tab > after_click);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_ESCAPE, GLFW_PRESS, 0));
    assert(g_platform_sync_calls > after_tab);

    std::puts("PASS: focus transitions sync platform input");
}

static void test_shell_scroll_and_escape_observability() {
    using namespace input_regression;

    Harness harness;

    assert(phenotype::detail::get_total_height() > harness.host.canvas_height());

    assert(phenotype::native::detail::dispatch_scroll(-1.0, harness.host.canvas_height()));
    float after_wheel = phenotype::detail::get_scroll_y();
    assert(after_wheel > 0.0f);
    assert(has_metric("scroll", "wheel", "handled"));

    phenotype::detail::set_focus_id(button_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_PAGE_DOWN, GLFW_PRESS, 0));
    float after_page_down = phenotype::detail::get_scroll_y();
    assert(after_page_down > after_wheel);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_DOWN, GLFW_PRESS, 0));
    float after_down = phenotype::detail::get_scroll_y();
    assert(after_down > after_page_down);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_END, GLFW_PRESS, 0));
    float max_scroll = phenotype::native::detail::max_scroll_for_viewport(
        harness.host.canvas_height());
    assert(std::fabs(phenotype::detail::get_scroll_y() - max_scroll) < 0.001f);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_HOME, GLFW_PRESS, 0));
    assert(phenotype::detail::get_scroll_y() == 0.0f);

    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    float before_input_up = phenotype::detail::get_scroll_y();
    assert(!phenotype::native::detail::dispatch_key(GLFW_KEY_UP, GLFW_PRESS, 0));
    assert(phenotype::detail::get_scroll_y() == before_input_up);
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.detail == "arrow-up");
    assert(debug.result == "ignored");
    assert(debug.focused_role == "text_field");

    phenotype::detail::set_focus_id(button_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_ESCAPE, GLFW_PRESS, 0));
    assert(phenotype::detail::get_focused_id() == phenotype::native::invalid_callback_id);
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.detail == "escape");
    assert(debug.result == "handled");
    assert(has_metric("key", "escape", "handled"));
    std::puts("PASS: shared shell scroll and escape observability");
}

// ============================================================
// CoreText text tests
// ============================================================

#ifdef __APPLE__

static void test_macos_utf16_utf8_range_helpers() {
    using phenotype::native::macos_test::build_visual_text;
    using phenotype::native::macos_test::utf16_range_to_utf8;
    using phenotype::native::macos_test::utf8_prefix_to_utf16;

    auto bytes = utf16_range_to_utf8("A찬B", 1, 1);
    assert(bytes.start == 1);
    assert(bytes.end == 4);

    auto prefix_units = utf8_prefix_to_utf16("가나다", 3);
    assert(prefix_units == 1);

    auto visual = build_visual_text("ABCD", 1, 3, "XY", 1);
    assert(visual.visible_text == "AXYD");
    assert(visual.marked_start == 1);
    assert(visual.marked_end == 3);
    assert(visual.caret_bytes == 2);

    std::puts("PASS: macOS utf16/utf8 range helpers");
}

struct MacRendererFixture {
    GLFWwindow* window = nullptr;
    native_host host{};

    MacRendererFixture() {
        assert(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(320, 240, "phenotype-test", nullptr, nullptr);
        assert(window != nullptr);

        text::init();
        renderer::init(window);

        host.window = window;
        host.platform = &current_platform();
    }

    ~MacRendererFixture() {
        renderer::shutdown();
        text::shutdown();
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
    }
};

struct MacInputHarness {
    GLFWwindow* window = nullptr;
    native_host host{};

    MacInputHarness() {
        input_regression::reset_core_state();
        assert(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(360, 640, "phenotype-input-test", nullptr, nullptr);
        assert(window != nullptr);

        host.window = window;
        host.platform = &current_platform();
        phenotype::native::run<input_regression::State, input_regression::Msg>(
            host,
            input_regression::view,
            input_regression::update);
    }

    ~MacInputHarness() {
        phenotype::native::detail::shutdown_host(host);
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        phenotype::native::macos_test::force_disable_system_caret(false);
        input_regression::reset_core_state();
    }

    std::pair<float, float> point_for(unsigned int callback_id) const {
        for (float y = 0.0f; y <= 740.0f; y += 2.0f) {
            for (float x = 0.0f; x <= 360.0f; x += 2.0f) {
                auto hit = renderer::hit_test(
                    x, y, phenotype::detail::get_scroll_y());
                if (hit.has_value() && *hit == callback_id)
                    return {x, y};
            }
        }
        assert(false && "missing hit-test point");
        return {0.0f, 0.0f};
    }
};

static void test_macos_system_caret_indicator_tracks_focus_and_composition() {
    using namespace input_regression;
    using phenotype::native::macos_test::clear_composition_for_tests;
    using phenotype::native::macos_test::force_disable_system_caret;
    using phenotype::native::macos_test::set_composition_for_tests;
    using phenotype::native::macos_test::system_caret_debug;

    force_disable_system_caret(false);
    MacInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    auto caret = system_caret_debug();
    assert(caret.supported);
    assert(caret.attached);
    assert(caret.display_mode == 0);
    assert(caret.frame.valid);
    assert(caret.draw_rect.valid);
    assert(caret.host_rect.valid);
    assert(caret.screen_rect.valid);
    assert(caret.host_bounds.valid);
    assert(caret.host_flipped);
    assert(caret.first_rect_screen.valid);
    assert(std::fabs(caret.frame.x - caret.host_rect.x) < 0.001f);
    assert(std::fabs(caret.frame.y - caret.host_rect.y) < 0.001f);
    assert(std::fabs(caret.frame.w - caret.host_rect.w) < 0.001f);
    assert(std::fabs(caret.frame.h - caret.host_rect.h) < 0.001f);
    assert(std::fabs(caret.screen_rect.x - caret.first_rect_screen.x) < 1.0f);
    assert(std::fabs(caret.screen_rect.y - caret.first_rect_screen.y) < 1.0f);
    assert(std::fabs(caret.screen_rect.h - caret.first_rect_screen.h) < 1.0f);

    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.caret_renderer == "system");
    assert(debug.caret_rect.valid);
    assert(debug.caret_draw_rect.valid);
    assert(debug.caret_host_rect.valid);
    assert(debug.caret_screen_rect.valid);
    assert(debug.caret_host_bounds.valid);
    assert(debug.caret_host_flipped);
    assert(debug.caret_geometry_source == "draw");
    assert(std::fabs(debug.caret_rect.y - debug.caret_draw_rect.y) < 0.001f);
    assert(std::fabs(debug.caret_host_rect.y - caret.frame.y) < 0.001f);

    assert(phenotype::native::detail::set_scroll_position(
        48.0f,
        harness.host.canvas_height(),
        "test-scroll"));
    caret = system_caret_debug();
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.caret_renderer == "system");
    assert(debug.caret_rect.valid);
    assert(debug.caret_draw_rect.valid);
    assert(debug.caret_host_rect.valid);
    assert(std::fabs(debug.caret_draw_rect.y - debug.caret_host_rect.y) < 0.001f);
    assert(std::fabs(caret.frame.y - debug.caret_host_rect.y) < 0.001f);
    assert(std::fabs(caret.draw_rect.y - debug.caret_draw_rect.y) < 0.001f);
    assert(std::fabs(caret.host_rect.y - debug.caret_host_rect.y) < 0.001f);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_ESCAPE, GLFW_PRESS, 0));
    caret = system_caret_debug();
    assert(caret.display_mode == 1);
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.caret_renderer == "hidden");
    assert(!debug.caret_rect.valid);
    assert(!debug.caret_draw_rect.valid);
    assert(!debug.caret_host_rect.valid);
    assert(!debug.caret_screen_rect.valid);

    phenotype::detail::set_scroll_y(0.0f);
    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    harness.host.platform->input.sync();
    set_composition_for_tests("가", 0, 0, 1);
    harness.host.platform->input.sync();
    caret = system_caret_debug();
    assert(caret.display_mode == 1);
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.caret_renderer == "hidden");
    clear_composition_for_tests();

    std::puts("PASS: macOS system caret indicator tracks focus and composition");
}

static void test_macos_fallback_caret_path_exposes_custom_debug_rect() {
    using namespace input_regression;
    using phenotype::native::macos_test::force_disable_system_caret;
    using phenotype::native::macos_test::system_caret_debug;

    force_disable_system_caret(true);
    MacInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));
    phenotype::native::detail::repaint_current();

    auto caret = system_caret_debug();
    assert(!caret.supported);
    assert(!caret.attached);
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.caret_renderer == "custom");
    assert(debug.caret_rect.valid);
    assert(debug.caret_draw_rect.valid);
    assert(debug.caret_geometry_source == "draw");
    assert(harness.host.platform->input.uses_shared_caret_blink());

    std::puts("PASS: macOS fallback caret path exposes custom debug rect");
}

static void test_text_measure_basic() {
    text::init();
    float w = text::measure(16.0f, false, "Hello", 5);
    assert(w > 0.f);
    assert(w < 200.f);

    float w2 = text::measure(16.0f, false, "Helloworld", 10);
    assert(w2 > w);
    text::shutdown();
    std::puts("PASS: text measure basic");
}

static void test_text_measure_proportional() {
    text::init();
    float wi = text::measure(16.0f, false, "mmmm", 4);
    float wm = text::measure(16.0f, false, "iiii", 4);
    assert(wi > 0.f);
    assert(wm > 0.f);
    assert(wi != wm); // proportional font: 'm' and 'i' have different widths
    text::shutdown();
    std::puts("PASS: text measure proportional");
}

static void test_text_measure_mono_fixed_width() {
    text::init();
    float w1 = text::measure(16.0f, true, "i", 1);
    float w2 = text::measure(16.0f, true, "ii", 2);
    assert(w1 > 0.f);
    assert(std::fabs(w2 - 2.0f * w1) < 0.5f); // mono: width scales linearly
    text::shutdown();
    std::puts("PASS: text measure mono fixed width");
}

static void test_text_measure_empty() {
    text::init();
    float w = text::measure(16.0f, false, "", 0);
    assert(w == 0.f);
    text::shutdown();
    std::puts("PASS: text measure empty");
}

static void test_text_measure_unicode() {
    text::init();
    // Korean characters (UTF-8: 3 bytes each)
    float w = text::measure(16.0f, false, "\xec\x95\x88\xeb\x85\x95", 6);
    assert(w > 0.f);
    text::shutdown();
    std::puts("PASS: text measure unicode");
}

static void test_text_measure_uses_kerning_pairs() {
    text::init();

    float a = text::measure(16.0f, false, "A", 1);
    float v = text::measure(16.0f, false, "V", 1);
    float av = text::measure(16.0f, false, "AV", 2);
    assert(av > 0.f);
    assert(av + 0.25f < a + v);

    float t = text::measure(16.0f, false, "T", 1);
    float o = text::measure(16.0f, false, "o", 1);
    float to = text::measure(16.0f, false, "To", 2);
    assert(to > 0.f);
    assert(to + 0.25f < t + o);

    text::shutdown();
    std::puts("PASS: text measure uses kerning pairs");
}

static void test_text_build_atlas() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({10.f, 20.f, 16.f, false, 0.f, 0.f, 0.f, 1.f, "Hi", 16.f * 1.6f});
    auto atlas = text::build_atlas(entries);
    assert(!atlas.pixels.empty());
    assert(atlas.width > 0);
    assert(atlas.height > 0);
    assert(atlas.quads.size() == 1);
    assert(atlas.quads[0].u >= 0.f && atlas.quads[0].u <= 1.f);
    assert(atlas.quads[0].v >= 0.f && atlas.quads[0].v <= 1.f);
    text::shutdown();
    std::puts("PASS: text build atlas");
}

static void test_text_build_atlas_crops_padding() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({10.f, 20.f, 16.f, false, 0.f, 0.f, 0.f, 1.f, "Hi", 16.f * 1.6f});
    auto atlas = text::build_atlas(entries, 2.0f);
    assert(atlas.quads.size() == 1);

    float atlas_px_w = atlas.quads[0].uw * static_cast<float>(atlas.width);
    float atlas_px_h = atlas.quads[0].vh * static_cast<float>(atlas.height);
    assert(std::fabs(atlas_px_w - atlas.quads[0].w * 2.0f) < 1.1f);
    assert(std::fabs(atlas_px_h - atlas.quads[0].h * 2.0f) < 1.1f);

    text::shutdown();
    std::puts("PASS: text build atlas crops padding");
}

static void test_text_build_atlas_scale_preserves_logical_bounds() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({10.f, 20.f, 16.f, false, 0.f, 0.f, 0.f, 1.f, "Hello", 16.f * 1.6f});

    auto atlas1 = text::build_atlas(entries, 1.0f);
    auto atlas2 = text::build_atlas(entries, 2.0f);
    assert(atlas1.quads.size() == 1);
    assert(atlas2.quads.size() == 1);

    float px_w1 = atlas1.quads[0].uw * static_cast<float>(atlas1.width);
    float px_w2 = atlas2.quads[0].uw * static_cast<float>(atlas2.width);
    assert(px_w2 > px_w1 * 1.7f);
    assert(std::fabs(atlas1.quads[0].x - atlas2.quads[0].x) < 0.6f);
    assert(atlas1.quads[0].w > 0.f);
    assert(atlas2.quads[0].w > 0.f);
    assert(atlas1.quads[0].h > 0.f);
    assert(atlas2.quads[0].h > 0.f);

    text::shutdown();
    std::puts("PASS: text build atlas scale preserves logical bounds");
}

static void test_text_build_atlas_mixed_fallback_scale_preserves_bounds() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({
        10.f,
        20.f,
        16.f,
        false,
        0.f,
        0.f,
        0.f,
        1.f,
        "Hello \xec\x95\x88\xeb\x85\x95",
        16.f * 1.6f
    });

    auto atlas1 = text::build_atlas(entries, 1.0f);
    auto atlas2 = text::build_atlas(entries, 2.0f);
    assert(atlas1.quads.size() == 1);
    assert(atlas2.quads.size() == 1);

    float px_w1 = atlas1.quads[0].uw * static_cast<float>(atlas1.width);
    float px_w2 = atlas2.quads[0].uw * static_cast<float>(atlas2.width);
    assert(px_w2 > px_w1 * 1.7f);
    assert(std::fabs(atlas1.quads[0].x - atlas2.quads[0].x) < 0.6f);
    assert(std::fabs(atlas1.quads[0].y - atlas2.quads[0].y) < 0.6f);
    assert(std::fabs(atlas1.quads[0].h - atlas2.quads[0].h) < 1.2f);
    assert(atlas1.quads[0].w > 0.f);
    assert(atlas2.quads[0].w > 0.f);

    text::shutdown();
    std::puts("PASS: text build atlas mixed fallback scale preserves bounds");
}

static void test_text_build_atlas_respects_line_box() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({10.f, 20.f, 16.f, false, 0.f, 0.f, 0.f, 1.f, "Hello", 32.f});
    auto atlas = text::build_atlas(entries, 1.0f);
    assert(atlas.quads.size() == 1);
    assert(std::fabs(atlas.quads[0].y - 20.f) < 0.1f);
    assert(std::fabs(atlas.quads[0].h - 32.f) < 1.1f);

    text::shutdown();
    std::puts("PASS: text build atlas respects line box");
}

static void test_text_build_atlas_keeps_line_box_stable() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({10.f, 20.f, 16.f, false, 0.f, 0.f, 0.f, 1.f, "ttt", 32.f});
    entries.push_back({10.f, 60.f, 16.f, false, 0.f, 0.f, 0.f, 1.f, "ppp", 32.f});
    auto atlas = text::build_atlas(entries, 2.0f);
    assert(atlas.quads.size() == 2);
    assert(std::fabs(atlas.quads[0].y - 20.f) < 0.1f);
    assert(std::fabs(atlas.quads[1].y - 60.f) < 0.1f);
    assert(std::fabs(atlas.quads[0].h - atlas.quads[1].h) < 0.1f);
    assert(std::fabs(atlas.quads[0].h - 32.f) < 0.6f);

    text::shutdown();
    std::puts("PASS: text build atlas keeps line box stable");
}

static void test_text_build_atlas_empty() {
    text::init();
    std::vector<text::TextEntry> entries;
    auto atlas = text::build_atlas(entries);
    assert(atlas.pixels.empty());
    assert(atlas.quads.empty());
    text::shutdown();
    std::puts("PASS: text build atlas empty");
}

static void test_text_init_shutdown_cycle() {
    for (int i = 0; i < 3; ++i) {
        text::init();
        float w = text::measure(16.0f, false, "test", 4);
        assert(w > 0.f);
        text::shutdown();
    }
    std::puts("PASS: text init/shutdown cycle");
}

static void test_macos_renderer_uploads_image_texture() {
    MacRendererFixture fixture;

    auto repo_root = std::filesystem::path(__FILE__).parent_path().parent_path();
    auto example_root = repo_root / "examples" / "native";
    auto image_path = example_root / "assets" / "showcase.bmp";
    assert(std::filesystem::exists(image_path));

    metrics::inst::native_texture_upload_bytes.reset();
    auto previous_cwd = std::filesystem::current_path();
    std::filesystem::current_path(example_root);

    std::vector<unsigned char> commands;
    append_u32(commands, static_cast<unsigned int>(Cmd::Clear));
    append_u32(commands, Color{245, 245, 245, 255}.packed());
    append_u32(commands, static_cast<unsigned int>(Cmd::DrawImage));
    append_f32(commands, 16.0f);
    append_f32(commands, 24.0f);
    append_f32(commands, 320.0f);
    append_f32(commands, 180.0f);
    auto image_string = std::string("assets/showcase.bmp");
    append_u32(commands, static_cast<unsigned int>(image_string.size()));
    append_bytes(
        commands,
        image_string.data(),
        static_cast<unsigned int>(image_string.size()));

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
    std::filesystem::current_path(previous_cwd);
    assert(metrics::inst::native_texture_upload_bytes.total() > 0);
    std::puts("PASS: macOS renderer uploads local image texture");
}

// ============================================================
// Stub backend contract tests
// ============================================================

#elif defined(_WIN32)

struct InputMsg {
    std::string value;
};

struct InputState {
    std::string value;
};

static InputMsg map_input(std::string value) {
    return {std::move(value)};
}

static void update_input(InputState& state, InputMsg msg) {
    state.value = std::move(msg.value);
}

static void view_input(InputState const& state) {
    phenotype::widget::text_field<InputMsg>("Hint", state.value, map_input);
}

static void test_windows_text_measure_basic() {
    text::init();
    float w = text::measure(16.0f, false, "Hello", 5);
    assert(w > 0.f);
    text::shutdown();
    std::puts("PASS: windows text measure basic");
}

static void test_windows_text_measure_proportional() {
    text::init();
    float wm = text::measure(16.0f, false, "mmmm", 4);
    float wi = text::measure(16.0f, false, "iiii", 4);
    assert(wm > 0.f);
    assert(wi > 0.f);
    assert(wm != wi);
    text::shutdown();
    std::puts("PASS: windows text measure proportional");
}

static void test_windows_text_measure_mono_fixed_width() {
    text::init();
    float w1 = text::measure(16.0f, true, "i", 1);
    float w2 = text::measure(16.0f, true, "ii", 2);
    assert(w1 > 0.f);
    assert(std::fabs(w2 - 2.0f * w1) < 1.0f);
    text::shutdown();
    std::puts("PASS: windows text measure mono fixed width");
}

static void test_windows_text_measure_unicode() {
    text::init();
    float w = text::measure(16.0f, false, "\xec\x95\x88\xeb\x85\x95", 6);
    assert(w > 0.f);
    text::shutdown();
    std::puts("PASS: windows text measure unicode");
}

static void test_windows_text_build_atlas() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({10.f, 20.f, 16.f, false, 0.f, 0.f, 0.f, 1.f, "Hello", 16.f * 1.6f});
    auto atlas = text::build_atlas(entries);
    assert(!atlas.pixels.empty());
    assert(atlas.quads.size() == 1);
    text::shutdown();
    std::puts("PASS: windows text build atlas");
}

static void test_windows_text_build_atlas_scale_preserves_bounds() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({10.f, 20.f, 16.f, false, 0.f, 0.f, 0.f, 1.f, "Scale", 16.f * 1.6f});
    auto atlas1 = text::build_atlas(entries, 1.0f);
    auto atlas2 = text::build_atlas(entries, 2.0f);
    assert(atlas1.quads.size() == 1);
    assert(atlas2.quads.size() == 1);
    float px_w1 = atlas1.quads[0].uw * static_cast<float>(atlas1.width);
    float px_w2 = atlas2.quads[0].uw * static_cast<float>(atlas2.width);
    assert(px_w2 > px_w1 * 1.7f);
    assert(std::fabs(atlas1.quads[0].x - atlas2.quads[0].x) < 0.6f);
    text::shutdown();
    std::puts("PASS: windows text build atlas scale preserves bounds");
}

static void test_windows_text_build_atlas_empty() {
    text::init();
    std::vector<text::TextEntry> entries;
    auto atlas = text::build_atlas(entries);
    assert(atlas.pixels.empty());
    assert(atlas.quads.empty());
    text::shutdown();
    std::puts("PASS: windows text build atlas empty");
}

static void test_windows_renderer_uploads_local_image_texture() {
    WindowsRendererFixture fixture;

    metrics::inst::native_texture_upload_bytes.reset();
    auto commands = make_draw_image_commands(local_test_image_url());

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
    assert(metrics::inst::native_texture_upload_bytes.total() > 0);
    std::puts("PASS: windows renderer uploads local image texture");
}

static void test_windows_renderer_rejects_remote_image_uploads() {
    WindowsRendererFixture fixture;

    metrics::inst::native_texture_upload_bytes.reset();
    auto commands = make_draw_image_commands("http://127.0.0.1/showcase.bmp");

    for (int i = 0; i < 5; ++i) {
        renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
    }

    assert(metrics::inst::native_texture_upload_bytes.total() == 0);
    std::puts("PASS: windows renderer rejects remote image uploads");
}

static void test_windows_renderer_hit_test_and_smoke() {
    WindowsRendererFixture fixture;

    emit_clear(fixture.host, {240, 240, 240, 255});
    emit_round_rect(fixture.host, 16.0f, 16.0f, 92.0f, 36.0f, 8.0f, {0, 102, 204, 255});
    emit_draw_text(fixture.host, 40.0f, 26.0f, 16.0f, 0u, {255, 255, 255, 255}, "Primary", 7);
    emit_hit_region(fixture.host, 16.0f, 16.0f, 92.0f, 36.0f, 42u, 1u);
    emit_round_rect(fixture.host, 124.0f, 16.0f, 92.0f, 36.0f, 8.0f, {236, 72, 153, 255});
    emit_draw_text(fixture.host, 150.0f, 26.0f, 16.0f, 0u, {255, 255, 255, 255}, "Action", 6);
    emit_hit_region(fixture.host, 124.0f, 16.0f, 92.0f, 36.0f, 84u, 1u);
    fixture.host.flush();

    auto first = renderer::hit_test(32.0f, 30.0f, 0.0f);
    assert(first.has_value());
    assert(*first == 42u);

    auto second = renderer::hit_test(150.0f, 30.0f, 0.0f);
    assert(second.has_value());
    assert(*second == 84u);

    auto miss = renderer::hit_test(5.0f, 5.0f, 0.0f);
    assert(!miss.has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    std::puts("PASS: windows renderer hit test + smoke");
}

static void test_windows_renderer_reinit_cycle() {
    {
        WindowsRendererFixture fixture;
        unsigned char empty[4] = {};
        renderer::flush(empty, 0);
    }
    {
        WindowsRendererFixture fixture;
        unsigned char empty[4] = {};
        renderer::flush(empty, 0);
    }

    std::puts("PASS: windows renderer reinit cycle");
}

static void test_windows_renderer_rejects_truncated_hit_region() {
    WindowsRendererFixture fixture;

    emit_clear(fixture.host, {245, 245, 245, 255});
    emit_round_rect(fixture.host, 20.0f, 20.0f, 100.0f, 40.0f, 8.0f, {37, 99, 235, 255});
    emit_draw_text(fixture.host, 45.0f, 31.0f, 16.0f, 0u, {255, 255, 255, 255}, "Button", 6);
    emit_hit_region(fixture.host, 20.0f, 20.0f, 100.0f, 40.0f, 77u, 1u);
    fixture.host.flush();

    auto before = renderer::hit_test(50.0f, 40.0f, 0.0f);
    assert(before.has_value());
    assert(*before == 77u);

    std::vector<unsigned char> broken;
    append_u32(broken, static_cast<unsigned int>(Cmd::HitRegion));
    append_f32(broken, 10.0f);
    append_f32(broken, 20.0f);
    append_f32(broken, 80.0f);
    append_f32(broken, 30.0f);
    append_u32(broken, 99u);
    renderer::flush(broken.data(), static_cast<unsigned int>(broken.size()));

    auto after = renderer::hit_test(50.0f, 40.0f, 0.0f);
    assert(after.has_value());
    assert(*after == 77u);
    std::puts("PASS: windows renderer rejects truncated hit region");
}

static void test_windows_renderer_rejects_truncated_text_payload() {
    WindowsRendererFixture fixture;

    emit_clear(fixture.host, {245, 245, 245, 255});
    emit_hit_region(fixture.host, 24.0f, 24.0f, 96.0f, 32.0f, 55u, 1u);
    fixture.host.flush();

    auto before = renderer::hit_test(40.0f, 40.0f, 0.0f);
    assert(before.has_value());
    assert(*before == 55u);

    std::vector<unsigned char> broken;
    append_u32(broken, static_cast<unsigned int>(Cmd::DrawText));
    append_f32(broken, 32.0f);
    append_f32(broken, 48.0f);
    append_f32(broken, 16.0f);
    append_u32(broken, 0u);
    append_u32(broken, Color{0, 0, 0, 255}.packed());
    append_u32(broken, 5u);
    append_bytes(broken, "Hi", 2);
    renderer::flush(broken.data(), static_cast<unsigned int>(broken.size()));

    auto after = renderer::hit_test(40.0f, 40.0f, 0.0f);
    assert(after.has_value());
    assert(*after == 55u);
    std::puts("PASS: windows renderer rejects truncated text payload");
}

static void test_windows_text_field_key_dispatch() {
    phenotype::null_host host;
    phenotype::run<InputState, InputMsg>(host, view_input, update_input);

    phenotype::detail::set_focus_id(0);
    phenotype::detail::handle_key(0, static_cast<unsigned int>('A'));
    assert(phenotype::detail::g_app.input_handlers.size() == 1);
    assert(phenotype::detail::g_app.input_handlers[0].second.current == "A");

    phenotype::detail::handle_key(0, static_cast<unsigned int>('B'));
    assert(phenotype::detail::g_app.input_handlers[0].second.current == "AB");

    phenotype::detail::handle_key(1, 0);
    assert(phenotype::detail::g_app.input_handlers[0].second.current == "A");
    std::puts("PASS: windows text field key dispatch");
}

static void test_windows_scroll_delta_uses_system_settings() {
    auto const scroll_delta = windows_platform().input.scroll_delta_y;
    assert(scroll_delta != nullptr);

    constexpr float line_height = 25.6f;
    constexpr float viewport_height = 240.0f;

    UINT lines = 3;
    auto ok = SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    assert(ok != 0);

    float expected = 0.0f;
    if (lines == WHEEL_PAGESCROLL) {
        expected = viewport_height - line_height;
    } else if (lines != 0u) {
        expected = static_cast<float>(lines) * line_height;
    }

    float zero = scroll_delta(0.0, line_height, viewport_height);
    float forward = scroll_delta(1.0, line_height, viewport_height);
    float backward = scroll_delta(-1.0, line_height, viewport_height);

    assert(zero == 0.0f);
    assert(std::fabs(forward - expected) < 0.001f);
    assert(std::fabs(backward + expected) < 0.001f);
    std::puts("PASS: windows scroll delta uses system settings");
}

#else // !__APPLE__ && !_WIN32

static void test_stub_text_measure_basic() {
    text::init();
    float w = text::measure(16.0f, false, "Hello", 5);
    assert(w > 0.f);
    text::shutdown();
    std::puts("PASS: stub text measure basic");
}

static void test_stub_renderer_hit_test() {
    native_host host;
    host.platform = &current_platform();

    emit_hit_region(host, 10.0f, 20.0f, 50.0f, 30.0f, 42u, 0u);
    host.flush();

    auto hit = renderer::hit_test(20.0f, 30.0f, 0.0f);
    assert(hit.has_value());
    assert(*hit == 42u);

    auto miss = renderer::hit_test(5.0f, 5.0f, 0.0f);
    assert(!miss.has_value());

    renderer::shutdown();
    std::puts("PASS: stub renderer hit test");
}

#endif // __APPLE__ / _WIN32

int main() {
    test_shell_pointer_hover_click_and_tab_navigation();
    test_shell_activation_keys_respect_roles();
    test_shell_text_space_char_and_enter_behavior();
    test_shell_text_caret_navigation_and_backspace();
    test_shell_pointer_text_caret_placement_and_visibility_reset();
    test_shared_caret_debug_rect_tracks_layout();
    test_caret_overlay_state_invalidates_cached_frame_hash();
    test_shared_text_replacement_helper();
    test_focus_transitions_sync_platform_input();
    test_shell_scroll_and_escape_observability();
#ifdef __APPLE__
    test_macos_utf16_utf8_range_helpers();
    test_macos_system_caret_indicator_tracks_focus_and_composition();
    test_macos_fallback_caret_path_exposes_custom_debug_rect();
    test_default_scroll_delta_fallback();
    test_text_measure_basic();
    test_text_measure_proportional();
    test_text_measure_mono_fixed_width();
    test_text_measure_empty();
    test_text_measure_unicode();
    test_text_measure_uses_kerning_pairs();
    test_text_build_atlas();
    test_text_build_atlas_crops_padding();
    test_text_build_atlas_scale_preserves_logical_bounds();
    test_text_build_atlas_mixed_fallback_scale_preserves_bounds();
    test_text_build_atlas_respects_line_box();
    test_text_build_atlas_keeps_line_box_stable();
    test_text_build_atlas_empty();
    test_text_init_shutdown_cycle();
    test_macos_renderer_uploads_image_texture();
    test_renderer_flush_empty();
    std::puts("\nAll native tests passed.");
#elif defined(_WIN32)
    test_default_scroll_delta_fallback();
    test_windows_text_measure_basic();
    test_windows_text_measure_proportional();
    test_windows_text_measure_mono_fixed_width();
    test_windows_text_measure_unicode();
    test_windows_text_build_atlas();
    test_windows_text_build_atlas_scale_preserves_bounds();
    test_windows_text_build_atlas_empty();
    test_renderer_flush_empty();
    test_windows_renderer_reinit_cycle();
    test_windows_renderer_uploads_local_image_texture();
    test_windows_renderer_rejects_remote_image_uploads();
    test_windows_renderer_hit_test_and_smoke();
    test_windows_renderer_rejects_truncated_hit_region();
    test_windows_renderer_rejects_truncated_text_payload();
    test_windows_text_field_key_dispatch();
    test_windows_scroll_delta_uses_system_settings();
    std::puts("\nAll Windows native tests passed.");
#else
    test_default_scroll_delta_fallback();
    test_stub_text_measure_basic();
    test_stub_renderer_hit_test();
    std::puts("\nAll stub native tests passed.");
#endif
    return 0;
}

#else

int main() {
    std::puts("SKIP: native backend tests are not available on wasi");
    return 0;
}

#endif

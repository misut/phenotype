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
#include <fstream>
#include <string>
#include <string_view>
#include <thread>
#include <variant>
#include <vector>

#if !defined(__wasi__) && !defined(__ANDROID__)

#include <GLFW/glfw3.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#ifdef DrawText
#undef DrawText
#endif
#endif

import phenotype.native;
import phenotype.native.stub;
import json;
#ifdef __APPLE__
import phenotype.native.macos;
#endif
#if defined(__APPLE__) || defined(_WIN32)
import cppx.http;
import cppx.http.server;
import cppx.http.system;
#endif
#ifdef _WIN32
import phenotype.native.windows;
#endif

using namespace phenotype::native;
using namespace phenotype;

static constexpr char kLocalExampleImageAsset[] = "showcase-local.bmp";
static constexpr char kRemoteExampleImageAsset[] = "showcase.bmp";

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

static std::filesystem::path make_debug_bundle_dir(std::string_view label) {
    auto stamp = std::to_string(
        std::chrono::steady_clock::now().time_since_epoch().count());
    auto path = std::filesystem::temp_directory_path()
        / ("phenotype-debug-" + std::string(label) + "-" + stamp);
    std::filesystem::remove_all(path);
    return path;
}

static std::string read_text_file(std::filesystem::path const& path) {
    std::ifstream in(path, std::ios::binary);
    assert(in.is_open());
    return std::string(
        std::istreambuf_iterator<char>(in),
        std::istreambuf_iterator<char>());
}

static bool file_exists_and_is_non_empty(std::filesystem::path const& path) {
    return std::filesystem::exists(path) && std::filesystem::file_size(path) > 0;
}

static void assert_non_empty_frame_capture(DebugFrameCapture const& frame) {
    assert(frame.width > 0);
    assert(frame.height > 0);
    assert(!frame.rgba.empty());
    assert(frame.rgba.size()
        == static_cast<std::size_t>(frame.width)
            * static_cast<std::size_t>(frame.height) * 4u);
    bool has_non_zero_channel = false;
    for (auto byte : frame.rgba) {
        if (byte != 0) {
            has_non_zero_channel = true;
            break;
        }
    }
    assert(has_non_zero_channel);
}

static float backing_scale_for_window(GLFWwindow* window) {
    assert(window != nullptr);
    int fbw = 0;
    int fbh = 0;
    int winw = 0;
    int winh = 0;
    glfwGetFramebufferSize(window, &fbw, &fbh);
    glfwGetWindowSize(window, &winw, &winh);
    assert(fbw > 0);
    assert(winw > 0);

    auto sx = static_cast<float>(fbw) / static_cast<float>(winw);
    auto sy = (winh > 0)
        ? static_cast<float>(fbh) / static_cast<float>(winh)
        : sx;
    return (sx > sy) ? sx : sy;
}

static int find_rightmost_dark_pixel_x(DebugFrameCapture const& frame,
                                       int min_x,
                                       int max_x,
                                       int min_y,
                                       int max_y) {
    min_x = std::max(0, min_x);
    min_y = std::max(0, min_y);
    max_x = std::min(static_cast<int>(frame.width), max_x);
    max_y = std::min(static_cast<int>(frame.height), max_y);
    if (max_x <= min_x || max_y <= min_y)
        return -1;

    int rightmost = -1;
    for (int y = min_y; y < max_y; ++y) {
        for (int x = min_x; x < max_x; ++x) {
            auto idx = static_cast<std::size_t>(
                (static_cast<unsigned int>(y) * frame.width
                    + static_cast<unsigned int>(x)) * 4u);
            auto r = frame.rgba[idx + 0];
            auto g = frame.rgba[idx + 1];
            auto b = frame.rgba[idx + 2];
            auto a = frame.rgba[idx + 3];
            if (a == 0)
                continue;
            if (r >= 170 || g >= 170 || b >= 170)
                continue;
            if (x > rightmost)
                rightmost = x;
        }
    }
    return rightmost;
}

static std::uint64_t sum_darkness(DebugFrameCapture const& frame,
                                  int min_x,
                                  int max_x,
                                  int min_y,
                                  int max_y) {
    min_x = std::max(0, min_x);
    min_y = std::max(0, min_y);
    max_x = std::min(static_cast<int>(frame.width), max_x);
    max_y = std::min(static_cast<int>(frame.height), max_y);
    if (max_x <= min_x || max_y <= min_y)
        return 0;

    std::uint64_t total = 0;
    for (int y = min_y; y < max_y; ++y) {
        for (int x = min_x; x < max_x; ++x) {
            auto idx = static_cast<std::size_t>(
                (static_cast<unsigned int>(y) * frame.width
                    + static_cast<unsigned int>(x)) * 4u);
            auto r = frame.rgba[idx + 0];
            auto g = frame.rgba[idx + 1];
            auto b = frame.rgba[idx + 2];
            auto a = frame.rgba[idx + 3];
            if (a == 0)
                continue;
            if (r >= 170 || g >= 170 || b >= 170)
                continue;
            total += static_cast<std::uint64_t>(255 - r);
            total += static_cast<std::uint64_t>(255 - g);
            total += static_cast<std::uint64_t>(255 - b);
        }
    }
    return total;
}

static bool remote_image_entry_matches(
        json::Object const& images,
        std::string const& url,
        std::string_view expected_state) {
    auto const& entries = images.at("remote_entries").as_array();
    for (auto const& value : entries) {
        auto const& entry = value.as_object();
        if (entry.at("url").as_string() != url)
            continue;
        if (!expected_state.empty())
            assert(entry.at("state").as_string() == expected_state);
        assert(entry.contains("failure_reason"));
        return true;
    }
    return false;
}

static void assert_macos_runtime_sections(json::Object const& details) {
    assert(details.contains("renderer"));
    assert(details.contains("images"));
    assert(details.contains("text_input"));

    auto const& renderer = details.at("renderer").as_object();
    assert(renderer.contains("initialized"));
    assert(renderer.contains("drawable_width"));
    assert(renderer.contains("drawable_height"));
    assert(renderer.contains("last_frame_available"));
    assert(renderer.contains("readback_texture_ready"));
    assert(renderer.contains("readback_buffer_ready"));

    auto const& images = details.at("images").as_object();
    assert(images.contains("pending_queue_count"));
    assert(images.contains("completed_queue_count"));
    assert(images.contains("worker_started"));
    assert(images.contains("remote_entry_count"));
    assert(images.contains("remote_entries"));

    auto const& text_input = details.at("text_input").as_object();
    assert(text_input.contains("system_caret"));
    assert(text_input.contains("composition"));
}

#ifdef _WIN32
struct WindowsRendererFixture {
    GLFWwindow* window = nullptr;
    native_host host{};

    WindowsRendererFixture() {
        _putenv_s("PHENOTYPE_WINDOWS_DEBUG", "0");
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

static void append_draw_text_command(
        std::vector<unsigned char>& commands,
        float x,
        float y,
        float font_size,
        bool mono,
        Color color,
        std::string const& text) {
    append_u32(commands, static_cast<unsigned int>(Cmd::DrawText));
    append_f32(commands, x);
    append_f32(commands, y);
    append_f32(commands, font_size);
    append_u32(commands, mono ? 1u : 0u);
    append_u32(commands, color.packed());
    append_u32(commands, static_cast<unsigned int>(text.size()));
    append_bytes(
        commands,
        text.data(),
        static_cast<unsigned int>(text.size()));
}

static std::vector<unsigned char> make_draw_image_and_heavy_text_commands(
        std::string const& image_url) {
    auto commands = make_draw_image_commands(image_url);
    for (unsigned int i = 0; i < 40; ++i) {
        std::string text = "Remote upload stress row ";
        text += std::to_string(i);
        text += " ";
        text.append(44, static_cast<char>('A' + (i % 26)));
        append_draw_text_command(
            commands,
            24.0f + static_cast<float>(i % 3) * 18.0f,
            230.0f + static_cast<float>(i) * 34.0f,
            44.0f + static_cast<float>(i % 4) * 4.0f,
            false,
            Color{20, 20, 20, 255},
            text);
    }
    return commands;
}

static std::string local_test_image_url() {
    auto image_path = native_example_root() / "assets" / kLocalExampleImageAsset;
    assert(std::filesystem::exists(image_path));
    return image_path.string();
}

static std::uint16_t find_free_local_port() {
    auto listener = cppx::http::system::listener::bind("127.0.0.1", 0);
    assert(listener);
    auto port = listener->local_port();
    listener->close();
    assert(port != 0);
    return port;
}

struct WindowsStaticHttpServer {
    std::uint16_t port = 0;
    cppx::http::server<cppx::http::system::listener, cppx::http::system::stream> server;
    std::thread thread;

    explicit WindowsStaticHttpServer(std::filesystem::path root) {
        port = find_free_local_port();
        server.serve_static("/", std::move(root));
        thread = std::thread([this] {
            auto result = server.run("127.0.0.1", port);
            assert(result);
        });
        wait_until_ready();
    }

    ~WindowsStaticHttpServer() {
        server.stop();
        (void)cppx::http::system::get(url("/__stop__"));
        if (thread.joinable())
            thread.join();
    }

    std::string url(std::string_view path) const {
        std::string value = "http://127.0.0.1:";
        value += std::to_string(port);
        if (path.empty() || path.front() != '/')
            value += '/';
        value += path;
        return value;
    }

    void wait_until_ready() const {
        auto ready_url = url(std::string("/") + kRemoteExampleImageAsset);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            auto response = cppx::http::system::get(ready_url);
            if (response && response->stat.code == 200)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        assert(false && "local HTTP server did not start in time");
    }
};

static auto wait_for_remote_image_terminal_state(
        std::vector<unsigned char> const& commands,
        std::string const& remote_url)
    -> phenotype::native::windows_test::RemoteImageDebug {
    using phenotype::native::windows_test::remote_image_debug;

    constexpr int ready_state = 1;
    constexpr int failed_state = 2;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
        auto debug = remote_image_debug(remote_url);
        if (debug.entry_exists
            && (debug.entry_state == ready_state || debug.entry_state == failed_state)) {
            return debug;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    assert(false && "remote image did not reach a terminal state in time");
    return {};
}

static void assert_dx12_renderer_clean(std::string_view context) {
    auto debug = phenotype::native::windows_test::dx12_diagnostics_debug();
    if (!debug.dred_enabled || debug.lost || debug.last_failure_hr != S_OK) {
        std::fprintf(
            stderr,
            "[test] unexpected DX12 diagnostics in %.*s: dred=%d lost=%d last_failure=0x%08lx removed=0x%08lx close=0x%08lx present=0x%08lx signal=0x%08lx label=%s\n",
            static_cast<int>(context.size()),
            context.data(),
            debug.dred_enabled ? 1 : 0,
            debug.lost ? 1 : 0,
            static_cast<unsigned long>(debug.last_failure_hr),
            static_cast<unsigned long>(debug.device_removed_reason),
            static_cast<unsigned long>(debug.last_close_hr),
            static_cast<unsigned long>(debug.last_present_hr),
            static_cast<unsigned long>(debug.last_signal_hr),
            debug.failure_label.c_str());
    }
    assert(debug.dred_enabled);
    assert(!debug.lost);
    assert(debug.last_failure_hr == S_OK);
    assert(debug.device_removed_reason == S_OK);
    assert(debug.last_close_hr == S_OK);
    assert(debug.last_present_hr == S_OK);
    assert(debug.last_signal_hr == S_OK);
    assert(debug.failure_label.empty());
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

static int select_all_mods() {
#ifdef __APPLE__
    return GLFW_MOD_SUPER;
#else
    return GLFW_MOD_CONTROL;
#endif
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
    app.selection_anchor = phenotype::native::invalid_callback_id;
    app.caret_visible = true;
    app.last_paint_hash = 0;
    app.debug_viewport_width = 0.0f;
    app.debug_viewport_height = 0.0f;
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

#ifdef _WIN32
struct WindowsInputHarness {
    GLFWwindow* window = nullptr;
    native_host host{};

    WindowsInputHarness() {
        input_regression::reset_core_state();
        phenotype::native::windows_test::reset_input_debug_counters();
        _putenv_s("PHENOTYPE_WINDOWS_DEBUG", "0");
        _putenv_s("PHENOTYPE_DX12_WARP", "1");
        _putenv_s("PHENOTYPE_DX12_DEBUG", "0");

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
        assert(phenotype::native::windows_test::attached_hwnd() != nullptr);
    }

    ~WindowsInputHarness() {
        phenotype::native::detail::shutdown_host(host);
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        phenotype::native::windows_test::reset_input_debug_counters();
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

namespace remote_shell_regression {

struct State {};
using Msg = std::monostate;

inline std::string g_remote_url;

static void update(State&, Msg) {}

static void view(State const&) {
    phenotype::layout::column([&] {
        phenotype::widget::text("Remote image shell stress");
        phenotype::layout::spacer(24);
        phenotype::widget::text(
            "This hidden-window regression exercises shell scroll, async image completion, and repaint integration.");
        phenotype::layout::spacer(1100);
        phenotype::layout::card([&] {
            phenotype::widget::text("Remote image");
            phenotype::layout::spacer(8);
            phenotype::widget::image(g_remote_url, 320.0f, 180.0f);
            phenotype::layout::spacer(12);
            phenotype::widget::text("The placeholder should swap cleanly after the worker completes.");
        });
        phenotype::layout::spacer(400);
        phenotype::widget::text("Bottom marker");
    });
}

} // namespace remote_shell_regression

struct WindowsRemoteShellHarness {
    GLFWwindow* window = nullptr;
    native_host host{};

    explicit WindowsRemoteShellHarness(std::string remote_url) {
        input_regression::reset_core_state();
        phenotype::native::windows_test::reset_input_debug_counters();
        _putenv_s("PHENOTYPE_WINDOWS_DEBUG", "0");
        _putenv_s("PHENOTYPE_DX12_WARP", "1");
        _putenv_s("PHENOTYPE_DX12_DEBUG", "0");

        remote_shell_regression::g_remote_url = std::move(remote_url);

        assert(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
        window = glfwCreateWindow(360, 640, "phenotype-remote-shell-test", nullptr, nullptr);
        assert(window != nullptr);

        host.window = window;
        host.platform = &current_platform();
        phenotype::native::run<remote_shell_regression::State, remote_shell_regression::Msg>(
            host,
            remote_shell_regression::view,
            remote_shell_regression::update);
        assert(phenotype::detail::get_total_height() > 640.0f);
    }

    ~WindowsRemoteShellHarness() {
        phenotype::native::detail::shutdown_host(host);
        if (window)
            glfwDestroyWindow(window);
        glfwTerminate();
        remote_shell_regression::g_remote_url.clear();
        phenotype::native::windows_test::reset_input_debug_counters();
        input_regression::reset_core_state();
    }
};
#endif

namespace {
int g_platform_sync_calls = 0;
int g_platform_sync_depth = 0;
int g_platform_sync_max_depth = 0;
bool g_request_repaint_during_sync_once = false;

void count_platform_sync() {
    ++g_platform_sync_calls;
}

void request_repaint_during_sync_once() {
    ++g_platform_sync_calls;
    ++g_platform_sync_depth;
    g_platform_sync_max_depth = (std::max)(g_platform_sync_max_depth, g_platform_sync_depth);
    if (g_request_repaint_during_sync_once) {
        g_request_repaint_during_sync_once = false;
        phenotype::native::detail::repaint_current();
    }
    --g_platform_sync_depth;
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

static void test_shell_text_selection_shortcuts_and_replacement() {
    using namespace input_regression;

    Harness harness;

    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('A')));
    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('B')));
    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('C')));
    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('D')));
    assert(g_observed_state.text == "ABCD");

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_A, GLFW_PRESS, select_all_mods()));
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.detail == "select-all");
    assert(debug.selection_active);
    assert(debug.selection_start == 0);
    assert(debug.selection_end == 4);

    assert(phenotype::native::detail::dispatch_char(static_cast<unsigned int>('Z')));
    assert(g_observed_state.text == "Z");
    debug = phenotype::diag::input_debug_snapshot();
    assert(!debug.selection_active);
    assert(debug.caret_pos == 1);

    assert(phenotype::detail::replace_focused_input_text(0, g_observed_state.text.size(), "A가C"));
    assert(g_observed_state.text == "A가C");

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_LEFT, GLFW_PRESS, GLFW_MOD_SHIFT));
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.selection_active);
    assert(debug.selection_start == 4);
    assert(debug.selection_end == 5);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_LEFT, GLFW_PRESS, GLFW_MOD_SHIFT));
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.selection_active);
    assert(debug.selection_start == 1);
    assert(debug.selection_end == 5);

    assert(phenotype::native::detail::dispatch_key(GLFW_KEY_BACKSPACE, GLFW_PRESS, 0));
    assert(g_observed_state.text == "A");
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.detail == "backspace");
    assert(debug.result == "handled");
    assert(!debug.selection_active);
    assert(debug.caret_pos == 1);
    std::puts("PASS: shared shell text selection shortcuts and replacement");
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

static void test_shell_pointer_drag_selection_and_click_collapse() {
    using namespace input_regression;

    Harness harness;

    phenotype::detail::set_focus_id(text_field_id, "test", "setup");
    assert(phenotype::detail::replace_focused_input_text(0, 0, "ABCD"));

    auto snapshot = phenotype::detail::focused_input_snapshot();
    assert(snapshot.valid);
    float y = snapshot.y + snapshot.height * 0.5f;
    float base_x = snapshot.x + snapshot.padding[3];
    auto prefix_width = [&](std::size_t bytes) {
        return harness.host.measure_text(
            snapshot.font_size,
            snapshot.mono ? 1u : 0u,
            snapshot.value.data(),
            static_cast<unsigned int>(bytes));
    };

    assert(phenotype::native::detail::dispatch_mouse_button(
        base_x + 1.0f,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));
    assert(phenotype::native::detail::dispatch_cursor_pos(
        base_x + prefix_width(3) + 1.0f,
        y));
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.detail == "pointer-drag");
    assert(debug.selection_active);
    assert(debug.selection_start == 0);
    assert(debug.selection_end == 3);

    assert(phenotype::native::detail::dispatch_mouse_button(
        base_x + prefix_width(3) + 1.0f,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_RELEASE,
        0));

    assert(phenotype::native::detail::dispatch_mouse_button(
        snapshot.x + snapshot.width - 1.0f,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));
    debug = phenotype::diag::input_debug_snapshot();
    assert(!debug.selection_active);
    assert(debug.caret_pos == 4);

    assert(phenotype::native::detail::dispatch_mouse_button(
        snapshot.x + snapshot.width + 40.0f,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.focused_id == phenotype::native::invalid_callback_id);
    assert(!debug.selection_active);
    std::puts("PASS: shared shell pointer drag selection and click collapse");
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

static void test_shell_repaint_coalesces_nested_requests() {
    using namespace input_regression;

    Harness harness;
    g_platform_sync_calls = 0;
    g_platform_sync_depth = 0;
    g_platform_sync_max_depth = 0;
    g_request_repaint_during_sync_once = true;
    harness.platform.input.sync = request_repaint_during_sync_once;

    phenotype::native::detail::repaint_current();

    assert(g_platform_sync_calls == 2);
    assert(g_platform_sync_max_depth == 1);
    assert(!g_request_repaint_during_sync_once);
    std::puts("PASS: shell repaint coalesces nested requests");
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

static void test_macos_scroll_delta_normalization() {
    using phenotype::native::macos_test::normalize_scroll_delta;

    float precise = normalize_scroll_delta(-12.5, true, 25.6f);
    float line = normalize_scroll_delta(-2.0, false, 25.6f);
    float default_line = normalize_scroll_delta(2.0, false, 0.0f);

    assert(std::fabs(precise + 12.5f) < 0.001f);
    assert(std::fabs(line + 51.2f) < 0.001f);
    assert(std::fabs(default_line - 2.0f) < 0.001f);
    std::puts("PASS: macOS scroll delta normalization");
}

static void test_macos_scroll_paths_record_precise_and_line_details() {
    using namespace input_regression;

    {
        Harness harness;
        assert(phenotype::detail::get_total_height() > harness.host.canvas_height());
        assert(phenotype::native::detail::dispatch_scroll_pixels(
            -12.5f,
            harness.host.canvas_height(),
            "wheel-precise"));
        auto debug = phenotype::diag::input_debug_snapshot();
        assert(debug.event == "scroll");
        assert(debug.detail == "wheel-precise");
        assert(debug.result == "handled");
        assert(std::fabs(phenotype::detail::get_scroll_y() - 12.5f) < 0.001f);
        assert(has_metric("scroll", "wheel-precise", "handled"));
    }

    {
        Harness harness;
        float line_height = phenotype::native::detail::scroll_line_height();
        assert(phenotype::native::detail::dispatch_scroll_lines(
            -2.0,
            harness.host.canvas_height(),
            "wheel-line"));
        auto debug = phenotype::diag::input_debug_snapshot();
        assert(debug.event == "scroll");
        assert(debug.detail == "wheel-line");
        assert(debug.result == "handled");
        assert(std::fabs(phenotype::detail::get_scroll_y() - line_height * 2.0f) < 0.001f);
        assert(has_metric("scroll", "wheel-line", "handled"));
    }

    std::puts("PASS: macOS scroll paths record precise and line details");
}

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
    auto image_path = native_example_root() / "assets" / kLocalExampleImageAsset;
    assert(std::filesystem::exists(image_path));
    return image_path.string();
}

static std::uint16_t find_free_local_port() {
    auto listener = cppx::http::system::listener::bind("127.0.0.1", 0);
    assert(listener);
    auto port = listener->local_port();
    listener->close();
    assert(port != 0);
    return port;
}

struct MacStaticHttpServer {
    std::uint16_t port = 0;
    cppx::http::server<cppx::http::system::listener, cppx::http::system::stream> server;
    std::thread thread;

    explicit MacStaticHttpServer(std::filesystem::path root) {
        port = find_free_local_port();
        server.serve_static("/", std::move(root));
        thread = std::thread([this] {
            auto result = server.run("127.0.0.1", port);
            assert(result);
        });
        wait_until_ready();
    }

    ~MacStaticHttpServer() {
        server.stop();
        (void)cppx::http::system::get(url("/__stop__"));
        if (thread.joinable())
            thread.join();
    }

    std::string url(std::string_view path) const {
        std::string value = "http://127.0.0.1:";
        value += std::to_string(port);
        if (path.empty() || path.front() != '/')
            value += '/';
        value += path;
        return value;
    }

    void wait_until_ready() const {
        auto ready_url = url(std::string("/") + kRemoteExampleImageAsset);
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (std::chrono::steady_clock::now() < deadline) {
            auto response = cppx::http::system::get(ready_url);
            if (response && response->stat.code == 200)
                return;
            std::this_thread::sleep_for(std::chrono::milliseconds(25));
        }
        assert(false && "local HTTP server did not start in time");
    }
};

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

static void test_macos_common_debug_contract_entry_points() {
    using namespace input_regression;
    using phenotype::native::macos_test::clear_composition_for_tests;
    using phenotype::native::macos_test::set_composition_for_tests;

    MacInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));
    set_composition_for_tests("im", 0, 0, 2);

    auto capabilities = phenotype::native::debug::capabilities();
    assert(capabilities.platform == "macos");
    assert(capabilities.read_only);
    assert(capabilities.snapshot_json);
    assert(capabilities.capture_frame_rgba);
    assert(capabilities.write_artifact_bundle);
    assert(capabilities.semantic_tree);
    assert(capabilities.input_debug);
    assert(capabilities.platform_runtime);
    assert(capabilities.frame_image);
    assert(capabilities.platform_diagnostics);

    auto snapshot_json = phenotype::native::debug::snapshot_json();
    auto snapshot = json::parse(snapshot_json);
    auto const& debug_payload = snapshot.as_object().at("debug").as_object();
    auto const& runtime = debug_payload.at("platform_runtime").as_object();
    assert(runtime.at("platform").as_string() == "macos");
    auto const& details = runtime.at("details").as_object();
    assert_macos_runtime_sections(details);
    auto const& renderer = details.at("renderer").as_object();
    assert(renderer.at("initialized").as_bool());
    assert(renderer.at("drawable_width").as_integer() > 0);
    assert(renderer.at("drawable_height").as_integer() > 0);
    auto const& images = details.at("images").as_object();
    assert(images.at("pending_queue_count").as_integer() == 0);
    assert(images.at("completed_queue_count").as_integer() == 0);
    assert(images.at("remote_entry_count").as_integer() == 0);
    auto const& text_input = details.at("text_input").as_object();
    auto const& system_caret = text_input.at("system_caret").as_object();
    assert(system_caret.contains("supported"));
    assert(system_caret.contains("frame"));
    auto const& composition = text_input.at("composition").as_object();
    assert(composition.at("active").as_bool());
    assert(composition.at("text").as_string() == "im");
    assert(composition.at("cursor_bytes").as_integer() == 2);

    auto bundle_dir = make_debug_bundle_dir("macos");
    auto bundle = phenotype::native::debug::write_artifact_bundle(
        bundle_dir.string(),
        "common-contract-test");
    assert(bundle.ok);
    assert(std::filesystem::exists(bundle_dir / "snapshot.json"));
    assert(std::filesystem::exists(bundle_dir / "platform" / "macos-runtime.json"));
    assert(bundle.platform_files.size() == 1);
    if (!bundle.frame_image_path.empty())
        assert(file_exists_and_is_non_empty(bundle_dir / "frame.bmp"));

    auto bundle_snapshot = json::parse(read_text_file(bundle_dir / "snapshot.json"));
    auto const& bundle_debug = bundle_snapshot.as_object().at("debug").as_object();
    assert(bundle_debug.contains("platform_capabilities"));
    assert(bundle_debug.contains("platform_runtime"));
    auto const& bundle_details = bundle_debug.at("platform_runtime").as_object()
        .at("details").as_object();
    assert_macos_runtime_sections(bundle_details);

    auto runtime_file = json::parse(
        read_text_file(bundle_dir / "platform" / "macos-runtime.json"));
    auto const& runtime_file_obj = runtime_file.as_object();
    assert(runtime_file_obj.contains("renderer"));
    assert(runtime_file_obj.contains("images"));
    assert(runtime_file_obj.contains("text_input"));
    assert(runtime_file_obj.at("artifact_reason").as_string() == "common-contract-test");

    std::filesystem::remove_all(bundle_dir);
    clear_composition_for_tests();
    std::puts("PASS: macOS common debug contract entry points");
}

static void test_macos_debug_capture_frame_from_rendered_frame() {
    MacRendererFixture fixture;

    auto commands = make_draw_image_commands(local_test_image_url());
    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto frame = phenotype::native::debug::capture_frame_rgba();
    assert(frame.has_value());
    assert_non_empty_frame_capture(*frame);
    std::puts("PASS: macOS debug capture returns RGBA for a rendered frame");
}

static void test_macos_rasterized_text_run_preserves_logical_end_for_latin_and_hangul() {
    using phenotype::native::macos_test::rasterized_text_run_debug;

    text::init();

    auto latin = rasterized_text_run_debug(
        "sadfasfasfasfasdfasdfasdfasdfasdfasdfa",
        16.0f,
        false,
        16.0f * 1.6f,
        2.0f);
    assert(latin.has_ink);
    assert(latin.x_offset < 0.25f);
    assert(latin.x_offset > -1.25f);
    assert(std::fabs(latin.right_gap) < 1.1f);
    assert(latin.first_ink_row > 0);
    assert(latin.last_ink_row >= latin.first_ink_row);
    assert(latin.last_ink_row < latin.pixel_height - 1);

    auto hangul = rasterized_text_run_debug(
        "\xec\x95\x88\xeb\x85\x95\xed\x95\x98\xec\x84\xb8\xec\x9a\x94\xec\x95\x88\xeb\x85\x95\xed\x95\x98\xec\x84\xb8\xec\x9a\x94",
        16.0f,
        false,
        16.0f * 1.6f,
        2.0f);
    assert(hangul.has_ink);
    assert(hangul.x_offset < 0.25f);
    assert(hangul.x_offset > -1.25f);
    assert(std::fabs(hangul.right_gap) < 1.1f);
    assert(hangul.first_ink_row > 0);
    assert(hangul.last_ink_row >= hangul.first_ink_row);
    assert(hangul.last_ink_row < hangul.pixel_height - 1);

    text::shutdown();
    std::puts("PASS: macOS rasterized text run preserves logical end for latin and hangul");
}

static void test_macos_system_caret_indicator_tracks_focus_and_composition() {
    using namespace input_regression;
    using phenotype::native::macos_test::clear_composition_for_tests;
    using phenotype::native::macos_test::force_disable_system_caret;
    using phenotype::native::macos_test::set_composition_for_tests;
    using phenotype::native::macos_test::system_caret_debug;

    constexpr long long show_effects = 1 << 0;
    constexpr long long show_while_tracking = 1 << 1;

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
    assert((caret.automatic_mode_options & show_effects) != 0);
    assert((caret.automatic_mode_options & show_while_tracking) != 0);
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
    assert((caret.automatic_mode_options & show_effects) != 0);
    assert((caret.automatic_mode_options & show_while_tracking) != 0);
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

static void test_macos_selected_range_and_selector_commands() {
    using namespace input_regression;
    using phenotype::native::macos_test::first_rect_for_range_debug;
    using phenotype::native::macos_test::invoke_command_for_tests;
    using phenotype::native::macos_test::perform_key_equivalent_for_tests;
    using phenotype::native::macos_test::selected_range_debug;

    MacInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    assert(phenotype::detail::replace_focused_input_text(0, 0, "ABCD"));
    invoke_command_for_tests("moveLeftAndModifySelection:");
    invoke_command_for_tests("moveLeftAndModifySelection:");

    auto selected = selected_range_debug();
    assert(selected.location_utf16 == 2);
    assert(selected.length_utf16 == 2);
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.selection_active);
    assert(debug.selection_start == 2);
    assert(debug.selection_end == 4);

    auto rect = first_rect_for_range_debug(selected.location_utf16, selected.length_utf16);
    assert(rect.valid);
    assert(rect.w >= 1.5f);

    invoke_command_for_tests("moveLeft:");
    selected = selected_range_debug();
    assert(selected.location_utf16 == 2);
    assert(selected.length_utf16 == 0);
    debug = phenotype::diag::input_debug_snapshot();
    assert(!debug.selection_active);
    assert(debug.caret_pos == 2);

    assert(perform_key_equivalent_for_tests(1ull << 20, "a"));
    selected = selected_range_debug();
    assert(selected.location_utf16 == 0);
    assert(selected.length_utf16 == 4);
    debug = phenotype::diag::input_debug_snapshot();
    assert(debug.selection_active);
    assert(debug.selection_start == 0);
    assert(debug.selection_end == 4);
    assert(debug.detail == "selectAll:");

    assert(phenotype::detail::set_focused_input_caret_pos(2));
    assert(perform_key_equivalent_for_tests(1ull << 20, "\xe3\x85\x81", 0));
    selected = selected_range_debug();
    assert(selected.location_utf16 == 0);
    assert(selected.length_utf16 == 4);
    std::puts("PASS: macOS selected range and selector commands");
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

static void test_macos_custom_caret_tracks_rightmost_text_pixel() {
    using namespace input_regression;
    using phenotype::native::macos_test::force_disable_system_caret;

    force_disable_system_caret(true);
    MacInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    std::string sample = "sadfasfasfasfasdfasdfasdfasdfasdfasdfa";
    assert(phenotype::detail::replace_focused_input_text(0, 0, sample));
    phenotype::native::detail::repaint_current();

    auto snapshot = phenotype::detail::focused_input_snapshot();
    assert(snapshot.valid);
    assert(snapshot.value == sample);

    auto debug = phenotype::diag::input_debug_snapshot();
    assert(debug.caret_renderer == "custom");
    assert(debug.caret_draw_rect.valid);

    auto frame = phenotype::native::debug::capture_frame_rgba();
    assert(frame.has_value());
    assert_non_empty_frame_capture(*frame);

    float scale = backing_scale_for_window(harness.window);
    int min_x = static_cast<int>(std::floor(
        (snapshot.x + snapshot.padding[3]) * scale));
    int max_x = static_cast<int>(std::ceil(
        (snapshot.x + snapshot.width - snapshot.padding[1]) * scale));
    int min_y = static_cast<int>(std::floor(
        (snapshot.y + snapshot.padding[0]) * scale));
    int max_y = static_cast<int>(std::ceil(
        (snapshot.y + snapshot.padding[0] + snapshot.line_height) * scale));

    auto rightmost_dark = find_rightmost_dark_pixel_x(
        *frame,
        min_x,
        max_x,
        min_y,
        max_y);
    assert(rightmost_dark >= 0);

    float rightmost_text_x = static_cast<float>(rightmost_dark + 1) / scale;
    assert(debug.caret_draw_rect.x + 0.75f >= rightmost_text_x);
    assert(debug.caret_draw_rect.x - rightmost_text_x < 2.5f);
    std::puts("PASS: macOS custom caret tracks rightmost text pixel");
}

static void test_macos_scroll_tracking_hides_caret_until_idle() {
    using namespace input_regression;
    using phenotype::native::macos_test::force_disable_system_caret;
    using phenotype::native::macos_test::set_scroll_tracking_for_tests;
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

    set_scroll_tracking_for_tests(true);
    harness.host.platform->input.sync();

    auto caret = system_caret_debug();
    auto debug = phenotype::diag::input_debug_snapshot();
    assert(caret.scroll_tracking_active);
    assert(caret.display_mode == 1);
    assert(debug.caret_renderer == "hidden");
    assert(!debug.caret_rect.valid);

    set_scroll_tracking_for_tests(false);
    harness.host.platform->input.sync();

    caret = system_caret_debug();
    debug = phenotype::diag::input_debug_snapshot();
    assert(!caret.scroll_tracking_active);
    assert(caret.display_mode == 0);
    assert(debug.caret_renderer == "system");
    assert(debug.caret_rect.valid);

    std::puts("PASS: macOS scroll tracking hides caret until idle");
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

static void test_macos_rendered_text_preserves_vertical_orientation() {
    MacRendererFixture fixture;

    std::vector<unsigned char> commands;
    append_u32(commands, static_cast<unsigned int>(Cmd::Clear));
    append_u32(commands, Color{255, 255, 255, 255}.packed());
    append_u32(commands, static_cast<unsigned int>(Cmd::DrawText));
    append_f32(commands, 24.0f);
    append_f32(commands, 24.0f);
    append_f32(commands, 96.0f);
    append_u32(commands, 0u);
    append_u32(commands, Color{0, 0, 0, 255}.packed());
    append_u32(commands, 1u);
    append_bytes(commands, "P", 1u);
    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto frame = phenotype::native::debug::capture_frame_rgba();
    assert(frame.has_value());
    assert_non_empty_frame_capture(*frame);

    float scale = backing_scale_for_window(fixture.window);
    int min_x = static_cast<int>(std::floor(24.0f * scale));
    int max_x = static_cast<int>(std::ceil((24.0f + 140.0f) * scale));
    int min_y = static_cast<int>(std::floor(24.0f * scale));
    int max_y = static_cast<int>(std::ceil((24.0f + 140.0f) * scale));

    int ink_min_x = max_x;
    int ink_min_y = max_y;
    int ink_max_x = -1;
    int ink_max_y = -1;
    for (int y = min_y; y < max_y; ++y) {
        for (int x = min_x; x < max_x; ++x) {
            auto idx = static_cast<std::size_t>(
                (static_cast<unsigned int>(y) * frame->width
                    + static_cast<unsigned int>(x)) * 4u);
            auto r = frame->rgba[idx + 0];
            auto g = frame->rgba[idx + 1];
            auto b = frame->rgba[idx + 2];
            auto a = frame->rgba[idx + 3];
            if (a == 0)
                continue;
            if (r >= 170 || g >= 170 || b >= 170)
                continue;
            ink_min_x = std::min(ink_min_x, x);
            ink_min_y = std::min(ink_min_y, y);
            ink_max_x = std::max(ink_max_x, x);
            ink_max_y = std::max(ink_max_y, y);
        }
    }

    assert(ink_max_x > ink_min_x);
    assert(ink_max_y > ink_min_y);

    int right_half_start = ink_min_x + (ink_max_x - ink_min_x + 1) / 2;
    int vertical_mid = ink_min_y + (ink_max_y - ink_min_y + 1) / 2;
    auto top_right_darkness = sum_darkness(
        *frame,
        right_half_start,
        ink_max_x + 1,
        ink_min_y,
        vertical_mid);
    auto bottom_right_darkness = sum_darkness(
        *frame,
        right_half_start,
        ink_max_x + 1,
        vertical_mid,
        ink_max_y + 1);
    assert(top_right_darkness > bottom_right_darkness);

    std::puts("PASS: macOS rendered text preserves vertical orientation");
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
    auto image_path = example_root / "assets" / kLocalExampleImageAsset;
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
    auto image_string = std::string("assets/") + kLocalExampleImageAsset;
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

static void test_macos_renderer_enqueues_remote_image_once_without_network() {
    using phenotype::native::macos_test::remote_image_debug;
    using phenotype::native::macos_test::reset_image_cache_for_tests;
    using phenotype::native::macos_test::set_image_queue_only_for_tests;

    constexpr int pending_state = 0;
    MacRendererFixture fixture;

    reset_image_cache_for_tests();
    set_image_queue_only_for_tests(true);
    metrics::inst::native_texture_upload_bytes.reset();

    auto remote_url = std::string("https://example.com/showcase.bmp");
    std::vector<unsigned char> commands;
    append_u32(commands, static_cast<unsigned int>(Cmd::Clear));
    append_u32(commands, Color{245, 245, 245, 255}.packed());
    append_u32(commands, static_cast<unsigned int>(Cmd::DrawImage));
    append_f32(commands, 16.0f);
    append_f32(commands, 24.0f);
    append_f32(commands, 320.0f);
    append_f32(commands, 180.0f);
    append_u32(commands, static_cast<unsigned int>(remote_url.size()));
    append_bytes(
        commands,
        remote_url.data(),
        static_cast<unsigned int>(remote_url.size()));

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto first = remote_image_debug(remote_url);
    assert(first.entry_exists);
    assert(first.entry_state == pending_state);
    assert(first.pending_jobs == 1);
    assert(first.completed_jobs == 0);
    assert(!first.worker_started);
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto second = remote_image_debug(remote_url);
    assert(second.entry_exists);
    assert(second.entry_state == pending_state);
    assert(second.pending_jobs == 1);
    assert(second.completed_jobs == 0);
    assert(!second.worker_started);
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);
    std::puts("PASS: macOS renderer enqueues remote image once without network");
}

static auto wait_for_macos_remote_image_terminal_state(
        std::vector<unsigned char> const& commands,
        std::string const& remote_url)
    -> phenotype::native::macos_test::RemoteImageDebug {
    using phenotype::native::macos_test::remote_image_debug;

    constexpr int ready_state = 1;
    constexpr int failed_state = 2;
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < deadline) {
        renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
        auto debug = remote_image_debug(remote_url);
        if (debug.entry_exists
            && (debug.entry_state == ready_state || debug.entry_state == failed_state)) {
            return debug;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
    assert(false && "macOS remote image did not reach a terminal state in time");
    return {};
}

static void test_macos_renderer_fetches_remote_image_via_worker() {
    using phenotype::native::macos_test::remote_image_debug;
    using phenotype::native::macos_test::reset_image_cache_for_tests;

    constexpr int pending_state = 0;
    constexpr int ready_state = 1;
    MacRendererFixture fixture;
    MacStaticHttpServer server(native_example_root() / "assets");

    reset_image_cache_for_tests();
    metrics::inst::native_texture_upload_bytes.reset();
    auto remote_url = server.url(std::string("/") + kRemoteExampleImageAsset);
    auto commands = make_draw_image_commands(remote_url);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto first = remote_image_debug(remote_url);
    assert(first.entry_exists);
    assert(first.entry_state == pending_state);
    assert(first.worker_started);
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);

    auto final = wait_for_macos_remote_image_terminal_state(commands, remote_url);
    assert(final.entry_exists);
    assert(final.entry_state == ready_state);
    assert(final.completed_jobs == 0);
    assert(metrics::inst::native_texture_upload_bytes.total() > 0);
    auto snapshot = json::parse(phenotype::native::debug::snapshot_json());
    auto const& details = snapshot.as_object().at("debug").as_object()
        .at("platform_runtime").as_object()
        .at("details").as_object();
    auto const& images = details.at("images").as_object();
    assert(images.at("worker_started").as_bool());
    assert(images.at("pending_queue_count").as_integer() >= 0);
    assert(images.at("completed_queue_count").as_integer() >= 0);
    assert(remote_image_entry_matches(images, remote_url, "ready"));

    reset_image_cache_for_tests();
    std::puts("PASS: macOS renderer fetches remote image via worker");
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

static void test_windows_text_build_atlas_preserves_vertical_orientation() {
    text::init();
    std::vector<text::TextEntry> entries;
    entries.push_back({10.f, 20.f, 40.f, false, 0.f, 0.f, 0.f, 1.f, "A", 40.f * 1.2f});
    auto atlas = text::build_atlas(entries);
    assert(atlas.quads.size() == 1);

    auto const& quad = atlas.quads[0];
    int atlas_x = static_cast<int>(std::floor(quad.u * static_cast<float>(atlas.width)));
    int atlas_y = static_cast<int>(std::floor(quad.v * static_cast<float>(atlas.height)));
    int atlas_w = static_cast<int>(std::ceil(quad.uw * static_cast<float>(atlas.width)));
    int atlas_h = static_cast<int>(std::ceil(quad.vh * static_cast<float>(atlas.height)));
    assert(atlas_w > 0);
    assert(atlas_h > 2);

    auto alpha_sum = [&](int begin_row, int end_row) {
        std::uint64_t total = 0;
        for (int y = begin_row; y < end_row; ++y) {
            for (int x = 0; x < atlas_w; ++x) {
                auto idx = static_cast<std::size_t>(
                    (((atlas_y + y) * atlas.width) + atlas_x + x) * 4 + 3);
                total += atlas.pixels[idx];
            }
        }
        return total;
    };

    int mid = atlas_h / 2;
    auto top_alpha = alpha_sum(0, mid);
    auto bottom_alpha = alpha_sum(mid, atlas_h);
    assert(bottom_alpha > top_alpha);

    text::shutdown();
    std::puts("PASS: windows text build atlas preserves vertical orientation");
}

static void test_windows_backing_scale_matches_glfw_content_scale() {
    assert(glfwInit());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
#ifdef GLFW_SCALE_TO_MONITOR
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif

    auto* window = glfwCreateWindow(320, 240, "phenotype-scale", nullptr, nullptr);
    assert(window != nullptr);

#if defined(GLFW_VERSION_MAJOR) \
    && ((GLFW_VERSION_MAJOR > 3) || (GLFW_VERSION_MAJOR == 3 && GLFW_VERSION_MINOR >= 3))
    float sx = 1.0f;
    float sy = 1.0f;
    glfwGetWindowContentScale(window, &sx, &sy);
    float expected = (sx > sy) ? sx : sy;
    float actual = phenotype::native::detail::glfw_backing_scale(window);
    assert(std::fabs(actual - expected) < 0.001f);
#else
    assert(phenotype::native::detail::glfw_backing_scale(window) > 0.0f);
#endif

    glfwDestroyWindow(window);
    glfwTerminate();
    std::puts("PASS: windows backing scale matches GLFW content scale");
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
    assert_dx12_renderer_clean("local image upload");
    std::puts("PASS: windows renderer uploads local image texture");
}

static void test_windows_debug_capture_frame_from_rendered_frame() {
    WindowsRendererFixture fixture;

    auto commands = make_draw_image_commands(local_test_image_url());
    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto frame = phenotype::native::debug::capture_frame_rgba();
    assert(frame.has_value());
    assert_non_empty_frame_capture(*frame);
    assert_dx12_renderer_clean("frame capture from rendered frame");
    std::puts("PASS: windows debug capture returns RGBA for a rendered frame");
}

static void test_windows_renderer_enables_dred_under_warp_fixture() {
    WindowsRendererFixture fixture;
    assert_dx12_renderer_clean("warp fixture initialization");
    std::puts("PASS: windows renderer enables DRED diagnostics under WARP");
}

static void test_windows_common_debug_contract_entry_points() {
    WindowsInputHarness harness;

    auto capabilities = phenotype::native::debug::capabilities();
    assert(capabilities.platform == "windows");
    assert(capabilities.read_only);
    assert(capabilities.snapshot_json);
    assert(capabilities.capture_frame_rgba);
    assert(capabilities.write_artifact_bundle);
    assert(capabilities.semantic_tree);
    assert(capabilities.input_debug);
    assert(capabilities.platform_runtime);
    assert(capabilities.frame_image);
    assert(capabilities.platform_diagnostics);

    auto snapshot_json = phenotype::native::debug::snapshot_json();
    auto snapshot = json::parse(snapshot_json);
    auto const& debug_payload = snapshot.as_object().at("debug").as_object();
    auto const& runtime = debug_payload.at("platform_runtime").as_object();
    assert(runtime.at("platform").as_string() == "windows");
    auto const& details = runtime.at("details").as_object();
    assert(details.contains("renderer"));
    assert(details.contains("ime"));
    assert(details.contains("images"));
    auto const& renderer_details = details.at("renderer").as_object();
    assert(renderer_details.at("last_frame_available").as_bool());
    assert(renderer_details.at("last_render_width").as_integer() > 0);
    assert(renderer_details.at("last_render_height").as_integer() > 0);
    auto const& images = details.at("images").as_object();
    assert(images.at("pending_queue_count").as_integer() == 0);
    assert(images.at("completed_queue_count").as_integer() == 0);
    assert(images.at("remote_entry_count").as_integer() == 0);

    auto frame = phenotype::native::debug::capture_frame_rgba();
    assert(frame.has_value());
    assert_non_empty_frame_capture(*frame);

    auto bundle_dir = make_debug_bundle_dir("windows");
    auto bundle = phenotype::native::debug::write_artifact_bundle(
        bundle_dir.string(),
        "common-contract-test");
    assert(bundle.ok);
    assert(std::filesystem::exists(bundle_dir / "snapshot.json"));
    assert(std::filesystem::exists(bundle_dir / "platform" / "windows-runtime.json"));
    assert(std::filesystem::exists(bundle_dir / "frame.bmp"));
    assert(!bundle.frame_image_path.empty());
    assert(bundle.platform_files.size() == 1);
    assert(file_exists_and_is_non_empty(bundle_dir / "frame.bmp"));

    auto bundle_snapshot = json::parse(read_text_file(bundle_dir / "snapshot.json"));
    auto const& bundle_debug = bundle_snapshot.as_object().at("debug").as_object();
    assert(bundle_debug.contains("platform_capabilities"));
    assert(bundle_debug.contains("semantic_tree"));
    auto const& bundle_details = bundle_debug.at("platform_runtime").as_object()
        .at("details").as_object();
    assert(bundle_details.contains("images"));

    auto runtime_file = json::parse(
        read_text_file(bundle_dir / "platform" / "windows-runtime.json"));
    auto const& runtime_file_obj = runtime_file.as_object();
    assert(runtime_file_obj.contains("renderer"));
    assert(runtime_file_obj.contains("ime"));
    assert(runtime_file_obj.contains("images"));
    assert(runtime_file_obj.at("artifact_reason").as_string() == "common-contract-test");

    std::filesystem::remove_all(bundle_dir);
    std::puts("PASS: windows common debug contract entry points");
}

static void test_windows_renderer_fetches_remote_image_via_worker() {
    using phenotype::native::windows_test::remote_image_debug;

    constexpr int pending_state = 0;
    constexpr int ready_state = 1;
    WindowsRendererFixture fixture;
    WindowsStaticHttpServer server(native_example_root() / "assets");

    metrics::inst::native_texture_upload_bytes.reset();
    auto remote_url = server.url("/showcase.bmp");
    auto commands = make_draw_image_commands(remote_url);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto first = remote_image_debug(remote_url);
    assert(first.entry_exists);
    assert(first.entry_state == pending_state);
    assert(first.worker_started);
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);

    auto final = wait_for_remote_image_terminal_state(commands, remote_url);
    assert(final.entry_exists);
    assert(final.entry_state == ready_state);
    assert(final.completed_jobs == 0);
    assert(metrics::inst::native_texture_upload_bytes.total() > 0);
    auto snapshot = json::parse(phenotype::native::debug::snapshot_json());
    auto const& details = snapshot.as_object().at("debug").as_object()
        .at("platform_runtime").as_object()
        .at("details").as_object();
    auto const& images = details.at("images").as_object();
    assert(images.at("worker_started").as_bool());
    assert(images.at("pending_queue_count").as_integer() >= 0);
    assert(images.at("completed_queue_count").as_integer() >= 0);
    assert(remote_image_entry_matches(images, remote_url, "ready"));
    assert_dx12_renderer_clean("remote image worker fetch");
    std::puts("PASS: windows renderer fetches remote image via worker");
}

static void test_windows_renderer_fetches_remote_image_with_large_text_uploads() {
    using phenotype::native::windows_test::remote_image_debug;

    constexpr int pending_state = 0;
    constexpr int ready_state = 1;
    WindowsRendererFixture fixture;
    WindowsStaticHttpServer server(native_example_root() / "assets");

    metrics::inst::native_texture_upload_bytes.reset();
    auto remote_url = server.url("/showcase.bmp");
    auto commands = make_draw_image_and_heavy_text_commands(remote_url);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto first = remote_image_debug(remote_url);
    assert(first.entry_exists);
    assert(first.entry_state == pending_state);
    assert(first.worker_started);

    auto final = wait_for_remote_image_terminal_state(commands, remote_url);
    assert(final.entry_exists);
    assert(final.entry_state == ready_state);
    assert(final.completed_jobs == 0);
    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
    assert_dx12_renderer_clean("remote image heavy text");
    std::puts("PASS: windows renderer fetches remote image with large text uploads");
}

static void test_windows_renderer_processes_remote_image_completion() {
    using phenotype::native::windows_test::has_pending_remote_image;
    using phenotype::native::windows_test::inject_completed_image;
    using phenotype::native::windows_test::remote_image_debug;
    using phenotype::native::windows_test::stop_image_worker;

    constexpr int pending_state = 0;
    constexpr int ready_state = 1;
    WindowsRendererFixture fixture;

    stop_image_worker();
    metrics::inst::native_texture_upload_bytes.reset();
    auto remote_url = std::string("http://127.0.0.1/showcase.bmp");
    auto commands = make_draw_image_commands(remote_url);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto first = remote_image_debug(remote_url);
    assert(first.entry_exists);
    assert(first.entry_state == pending_state);
    assert(first.pending_jobs == 1);
    assert(first.completed_jobs == 0);
    assert(!first.worker_started);
    assert(has_pending_remote_image(remote_url));
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto second = remote_image_debug(remote_url);
    assert(second.entry_exists);
    assert(second.entry_state == pending_state);
    assert(second.pending_jobs == 1);
    assert(second.completed_jobs == 0);
    assert(!second.worker_started);
    assert(has_pending_remote_image(remote_url));
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);

    inject_completed_image(
        remote_url,
        2,
        2);
    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto third = remote_image_debug(remote_url);
    assert(third.entry_exists);
    assert(third.entry_state == ready_state);
    assert(third.pending_jobs == 0);
    assert(third.completed_jobs == 0);
    assert(!has_pending_remote_image(remote_url));
    auto uploaded = metrics::inst::native_texture_upload_bytes.total();
    assert(uploaded > 0);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
    auto fourth = remote_image_debug(remote_url);
    assert(fourth.entry_exists);
    assert(fourth.entry_state == ready_state);
    assert(fourth.pending_jobs == 0);
    assert(fourth.completed_jobs == 0);
    assert(metrics::inst::native_texture_upload_bytes.total() == uploaded);
    assert_dx12_renderer_clean("remote image completion injection");
    std::puts("PASS: windows renderer processes remote image completion");
}

static void test_windows_renderer_keeps_failed_remote_image_placeholder() {
    using phenotype::native::windows_test::has_pending_remote_image;
    using phenotype::native::windows_test::inject_completed_image;
    using phenotype::native::windows_test::remote_image_debug;
    using phenotype::native::windows_test::stop_image_worker;

    constexpr int pending_state = 0;
    constexpr int failed_state = 2;
    WindowsRendererFixture fixture;

    stop_image_worker();
    metrics::inst::native_texture_upload_bytes.reset();
    auto remote_url = std::string("http://127.0.0.1/failing.bmp");
    auto commands = make_draw_image_commands(remote_url);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto first = remote_image_debug(remote_url);
    assert(first.entry_exists);
    assert(first.entry_state == pending_state);
    assert(first.pending_jobs == 1);
    assert(first.completed_jobs == 0);
    assert(has_pending_remote_image(remote_url));
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);

    inject_completed_image(
        remote_url,
        0,
        0,
        true,
        "Injected remote failure");
    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));

    auto second = remote_image_debug(remote_url);
    assert(second.entry_exists);
    assert(second.entry_state == failed_state);
    assert(second.pending_jobs == 0);
    assert(second.completed_jobs == 0);
    assert(!has_pending_remote_image(remote_url));
    assert(second.failure_reason == "Injected remote failure");
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);

    for (int i = 0; i < 3; ++i) {
        renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
    }

    auto third = remote_image_debug(remote_url);
    assert(third.entry_exists);
    assert(third.entry_state == failed_state);
    assert(third.pending_jobs == 0);
    assert(third.completed_jobs == 0);
    assert(metrics::inst::native_texture_upload_bytes.total() == 0);
    assert_dx12_renderer_clean("remote image failure placeholder");
    std::puts("PASS: windows renderer keeps failed remote image placeholder");
}

static void test_windows_renderer_remote_image_survives_resize_after_completion() {
    WindowsRendererFixture fixture;
    WindowsStaticHttpServer server(native_example_root() / "assets");

    auto remote_url = server.url("/showcase.bmp");
    auto commands = make_draw_image_and_heavy_text_commands(remote_url);

    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
    auto final = wait_for_remote_image_terminal_state(commands, remote_url);
    assert(final.entry_exists);
    assert(final.entry_state == 1);

    glfwSetWindowSize(fixture.window, 480, 360);
    glfwPollEvents();
    renderer::flush(commands.data(), static_cast<unsigned int>(commands.size()));
    assert_dx12_renderer_clean("remote image resize");
    std::puts("PASS: windows renderer survives resize after remote image completion");
}

static void test_windows_shell_remote_image_scroll_path_survives_async_completion() {
    using phenotype::native::windows_test::remote_image_debug;

    constexpr int ready_state = 1;
    constexpr int failed_state = 2;

    WindowsStaticHttpServer server(native_example_root() / "assets");
    auto remote_url = server.url("/showcase.bmp");
    WindowsRemoteShellHarness harness(remote_url);

    auto max_scroll = phenotype::native::detail::max_scroll_for_viewport(640.0f);
    auto target_scroll = (std::min)(max_scroll, 920.0f);
    assert(phenotype::native::detail::set_scroll_position(
        target_scroll,
        640.0f,
        "remote-shell-stress"));
    harness.host.platform->input.sync();

    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    phenotype::native::windows_test::RemoteImageDebug final{};
    while (std::chrono::steady_clock::now() < deadline) {
        glfwPollEvents();
        harness.host.platform->input.sync();
        final = remote_image_debug(remote_url);
        if (final.entry_exists
            && (final.entry_state == ready_state || final.entry_state == failed_state)) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }

    assert(final.entry_exists);
    assert(final.entry_state == ready_state || final.entry_state == failed_state);

    for (int i = 0; i < 20; ++i) {
        auto next = (i % 2 == 0)
            ? target_scroll
            : (std::max)(0.0f, target_scroll - 120.0f);
        (void)phenotype::native::detail::set_scroll_position(
            next,
            640.0f,
            "remote-shell-oscillation");
        glfwPollEvents();
        harness.host.platform->input.sync();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    assert_dx12_renderer_clean("shell remote scroll path");
    auto bundle_dir = make_debug_bundle_dir("windows-remote-shell");
    auto bundle = phenotype::native::debug::write_artifact_bundle(
        bundle_dir.string(),
        "remote-shell-scroll");
    assert(bundle.ok);
    assert(std::filesystem::exists(bundle_dir / "snapshot.json"));
    assert(std::filesystem::exists(bundle_dir / "platform" / "windows-runtime.json"));
    assert(std::filesystem::exists(bundle_dir / "frame.bmp"));
    assert(!bundle.frame_image_path.empty());
    assert(file_exists_and_is_non_empty(bundle_dir / "frame.bmp"));

    auto bundle_snapshot = json::parse(read_text_file(bundle_dir / "snapshot.json"));
    auto const& images = bundle_snapshot.as_object().at("debug").as_object()
        .at("platform_runtime").as_object()
        .at("details").as_object()
        .at("images").as_object();
    auto const expected_state = (final.entry_state == ready_state) ? "ready" : "failed";
    assert(remote_image_entry_matches(images, remote_url, expected_state));
    std::filesystem::remove_all(bundle_dir);
    std::puts("PASS: windows shell remote image scroll path survives async completion");
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

static void test_windows_ime_repaint_requests_are_deferred_until_sync() {
    using namespace input_regression;

    WindowsInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    auto hwnd = phenotype::native::windows_test::attached_hwnd();
    assert(hwnd != nullptr);

    phenotype::native::windows_test::reset_input_debug_counters();
    SendMessageW(hwnd, WM_IME_STARTCOMPOSITION, 0, 0);

    assert(phenotype::native::windows_test::repaint_request_count() == 1);
    assert(phenotype::native::windows_test::deferred_repaint_count() == 1);
    assert(phenotype::native::windows_test::repaint_pending());

    phenotype::native::detail::sync_platform_input();

    assert(!phenotype::native::windows_test::repaint_pending());
    assert(phenotype::native::windows_test::repaint_request_count() == 1);
    assert(phenotype::native::windows_test::deferred_repaint_count() == 1);
    assert(phenotype::native::windows_test::sync_call_count() == 2);
    std::puts("PASS: windows IME repaint requests are deferred until sync");
}

static void test_windows_ime_startcomposition_captures_current_selection_range() {
    using namespace input_regression;

    WindowsInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    assert(phenotype::detail::replace_focused_input_text(0, 0, "ABC"));
    assert(phenotype::detail::set_focused_input_selection(1, 3));

    auto hwnd = phenotype::native::windows_test::attached_hwnd();
    assert(hwnd != nullptr);

    phenotype::native::windows_test::clear_composition_for_tests();
    SendMessageW(hwnd, WM_IME_STARTCOMPOSITION, 0, 0);
    assert(phenotype::native::windows_test::composition_anchor() == 1);

    phenotype::native::windows_test::set_composition_for_tests(
        "Z",
        phenotype::native::windows_test::composition_anchor(),
        1,
        3);
    auto visual = phenotype::native::windows_test::composition_visual_debug();
    assert(visual.valid);
    assert(visual.visible_text == "AZ");
    assert(visual.erase_text == "ABC");
    assert(visual.marked_start == 1);
    assert(visual.marked_end == 2);
    assert(visual.caret_bytes == 2);

    phenotype::native::windows_test::clear_composition_for_tests();
    std::puts("PASS: windows IME start composition captures current selection range");
}

static void test_windows_ime_composition_visual_replaces_placeholder() {
    using namespace input_regression;

    WindowsInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    phenotype::native::windows_test::set_composition_for_tests("가", 0, 1);
    auto visual = phenotype::native::windows_test::composition_visual_debug();
    assert(visual.valid);
    assert(visual.composition_anchor == 0);
    assert(visual.erase_text == "Type here");
    assert(visual.visible_text == "가");
    assert(visual.marked_start == 0);
    assert(visual.marked_end == std::string("가").size());
    assert(visual.caret_bytes == std::string("가").size());

    phenotype::native::windows_test::clear_composition_for_tests();
    std::puts("PASS: windows IME composition visual replaces placeholder");
}

static void test_windows_ime_suppresses_base_placeholder_text_entry() {
    using namespace input_regression;

    WindowsInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    phenotype::native::windows_test::set_composition_for_tests("아", 0, 1);
    auto snapshot = phenotype::detail::focused_input_snapshot();
    assert(snapshot.valid);

    std::vector<phenotype::native::TextEntry> entries{
        {
            snapshot.x + snapshot.padding[3],
            snapshot.y - phenotype::detail::get_scroll_y() + snapshot.padding[0],
            snapshot.font_size,
            snapshot.mono,
            snapshot.placeholder_color.r / 255.0f,
            snapshot.placeholder_color.g / 255.0f,
            snapshot.placeholder_color.b / 255.0f,
            snapshot.placeholder_color.a / 255.0f,
            snapshot.placeholder,
            snapshot.line_height,
        },
        {
            12.0f,
            18.0f,
            16.0f,
            false,
            0.0f,
            0.0f,
            0.0f,
            1.0f,
            "keep me",
            25.6f,
        },
    };

    auto filtered = phenotype::native::windows_test::suppressed_text_entries_for_tests(
        std::move(entries));
    assert(filtered.size() == 1);
    assert(filtered[0].text == "keep me");

    phenotype::native::windows_test::clear_composition_for_tests();
    std::puts("PASS: windows IME suppresses base placeholder text entry");
}

static void test_windows_ime_overlay_text_omits_erase_pass() {
    using namespace input_regression;

    WindowsInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    phenotype::native::windows_test::set_composition_for_tests("아", 0, 1);
    auto entries = phenotype::native::windows_test::composition_overlay_text_entries_for_tests();
    assert(entries.size() == 1);
    assert(entries[0].text == "아");

    phenotype::native::windows_test::clear_composition_for_tests();
    std::puts("PASS: windows IME overlay text omits erase pass");
}

static void test_windows_ime_zero_cursor_draws_caret_at_composition_end() {
    using namespace input_regression;

    WindowsInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    assert(phenotype::detail::replace_focused_input_text(0, 0, "와우 친구들 "));
    phenotype::native::windows_test::set_composition_for_tests(
        "빡",
        g_observed_state.text.size(),
        0);

    auto visual = phenotype::native::windows_test::composition_visual_debug();
    assert(visual.valid);
    assert(visual.visible_text == std::string("와우 친구들 ") + "빡");
    assert(visual.marked_start == std::string("와우 친구들 ").size());
    assert(visual.marked_end == visual.visible_text.size());
    assert(visual.caret_bytes == visual.marked_end);

    phenotype::native::windows_test::clear_composition_for_tests();
    std::puts("PASS: windows IME zero cursor draws caret at composition end");
}

static void test_windows_ime_notify_self_generated_paths_do_not_request_repaint() {
    using namespace input_regression;

    WindowsInputHarness harness;
    auto [x, y] = harness.point_for(text_field_id);
    assert(phenotype::native::detail::dispatch_mouse_button(
        x,
        y,
        GLFW_MOUSE_BUTTON_LEFT,
        GLFW_PRESS,
        0));

    auto hwnd = phenotype::native::windows_test::attached_hwnd();
    assert(hwnd != nullptr);

    phenotype::native::windows_test::reset_input_debug_counters();
    SendMessageW(hwnd, WM_IME_NOTIFY, IMN_SETCOMPOSITIONWINDOW, 0);
    assert(phenotype::native::windows_test::repaint_request_count() == 0);
    assert(phenotype::native::windows_test::deferred_repaint_count() == 0);
    assert(!phenotype::native::windows_test::repaint_pending());

    phenotype::native::detail::sync_platform_input();
    assert(phenotype::native::windows_test::sync_call_count() == 1);
    assert(phenotype::native::windows_test::repaint_request_count() == 0);
    assert(phenotype::native::windows_test::deferred_repaint_count() == 0);
    assert(!phenotype::native::windows_test::repaint_pending());

    phenotype::native::windows_test::reset_input_debug_counters();
    SendMessageW(hwnd, WM_IME_NOTIFY, IMN_SETCANDIDATEPOS, 1);
    assert(phenotype::native::windows_test::repaint_request_count() == 0);
    assert(phenotype::native::windows_test::deferred_repaint_count() == 0);
    assert(!phenotype::native::windows_test::repaint_pending());

    phenotype::native::detail::sync_platform_input();
    assert(phenotype::native::windows_test::sync_call_count() == 1);
    assert(phenotype::native::windows_test::repaint_request_count() == 0);
    assert(phenotype::native::windows_test::deferred_repaint_count() == 0);
    assert(!phenotype::native::windows_test::repaint_pending());
    std::puts("PASS: windows IME self-generated notify paths do not request repaint");
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
    test_shell_text_selection_shortcuts_and_replacement();
    test_shell_pointer_text_caret_placement_and_visibility_reset();
    test_shell_pointer_drag_selection_and_click_collapse();
    test_shared_caret_debug_rect_tracks_layout();
    test_caret_overlay_state_invalidates_cached_frame_hash();
    test_shared_text_replacement_helper();
    test_focus_transitions_sync_platform_input();
    test_shell_repaint_coalesces_nested_requests();
    test_shell_scroll_and_escape_observability();
#ifdef __APPLE__
    test_macos_utf16_utf8_range_helpers();
    test_macos_scroll_delta_normalization();
    test_macos_scroll_paths_record_precise_and_line_details();
    test_macos_rasterized_text_run_preserves_logical_end_for_latin_and_hangul();
    test_macos_system_caret_indicator_tracks_focus_and_composition();
    test_macos_selected_range_and_selector_commands();
    test_macos_fallback_caret_path_exposes_custom_debug_rect();
    test_macos_custom_caret_tracks_rightmost_text_pixel();
    test_macos_scroll_tracking_hides_caret_until_idle();
    test_macos_common_debug_contract_entry_points();
    test_macos_debug_capture_frame_from_rendered_frame();
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
    test_macos_rendered_text_preserves_vertical_orientation();
    test_text_build_atlas_mixed_fallback_scale_preserves_bounds();
    test_text_build_atlas_respects_line_box();
    test_text_build_atlas_keeps_line_box_stable();
    test_text_build_atlas_empty();
    test_text_init_shutdown_cycle();
    test_macos_renderer_uploads_image_texture();
    test_macos_renderer_enqueues_remote_image_once_without_network();
    test_macos_renderer_fetches_remote_image_via_worker();
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
    test_windows_text_build_atlas_preserves_vertical_orientation();
    test_windows_backing_scale_matches_glfw_content_scale();
    test_windows_text_build_atlas_empty();
    test_renderer_flush_empty();
    test_windows_renderer_reinit_cycle();
    test_windows_renderer_uploads_local_image_texture();
    test_windows_debug_capture_frame_from_rendered_frame();
    test_windows_renderer_enables_dred_under_warp_fixture();
    test_windows_common_debug_contract_entry_points();
    test_windows_renderer_fetches_remote_image_via_worker();
    test_windows_renderer_fetches_remote_image_with_large_text_uploads();
    test_windows_renderer_processes_remote_image_completion();
    test_windows_renderer_keeps_failed_remote_image_placeholder();
    test_windows_renderer_remote_image_survives_resize_after_completion();
    test_windows_shell_remote_image_scroll_path_survives_async_completion();
    test_windows_renderer_hit_test_and_smoke();
    test_windows_renderer_rejects_truncated_hit_region();
    test_windows_renderer_rejects_truncated_text_payload();
    test_windows_text_field_key_dispatch();
    test_windows_scroll_delta_uses_system_settings();
    test_windows_ime_repaint_requests_are_deferred_until_sync();
    test_windows_ime_startcomposition_captures_current_selection_range();
    test_windows_ime_composition_visual_replaces_placeholder();
    test_windows_ime_suppresses_base_placeholder_text_entry();
    test_windows_ime_overlay_text_omits_erase_pass();
    test_windows_ime_zero_cursor_draws_caret_at_composition_end();
    test_windows_ime_notify_self_generated_paths_do_not_request_repaint();
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

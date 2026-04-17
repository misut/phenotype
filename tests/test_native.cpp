// Native backend tests — macOS CoreText/Metal + cross-platform stub contracts.
// CoreText tests run on any macOS (no GPU needed).
// Metal tests require a Metal device (SKIP if unavailable).

#include <cassert>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#ifndef __wasi__

#ifdef _WIN32
#include <GLFW/glfw3.h>
#endif

import phenotype.native;

using namespace phenotype::native;
using namespace phenotype;

static void test_renderer_flush_empty() {
    unsigned char buf[4] = {};
    renderer::flush(buf, 0);
    std::puts("PASS: renderer flush empty");
}

// ============================================================
// CoreText text tests
// ============================================================

#ifdef __APPLE__

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

static void test_windows_renderer_hit_test_and_smoke() {
    _putenv_s("PHENOTYPE_DX12_WARP", "1");
    _putenv_s("PHENOTYPE_DX12_DEBUG", "0");

    assert(glfwInit());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    auto* window = glfwCreateWindow(320, 240, "phenotype-test", nullptr, nullptr);
    assert(window != nullptr);

    text::init();
    renderer::init(window);

    native_host host;
    host.window = window;
    host.platform = &current_platform();

    emit_clear(host, {240, 240, 240, 255});
    emit_fill_rect(host, 8.0f, 8.0f, 40.0f, 30.0f, {255, 0, 0, 255});
    emit_round_rect(host, 64.0f, 12.0f, 80.0f, 28.0f, 6.0f, {0, 128, 255, 255});
    emit_draw_text(host, 16.0f, 64.0f, 16.0f, 0u, {0, 0, 0, 255}, "DX12", 4);
    emit_hit_region(host, 10.0f, 20.0f, 50.0f, 30.0f, 42u, 0u);
    host.flush();

    auto hit = renderer::hit_test(20.0f, 30.0f, 0.0f);
    assert(hit.has_value());
    assert(*hit == 42u);

    auto miss = renderer::hit_test(5.0f, 5.0f, 0.0f);
    assert(!miss.has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    renderer::shutdown();
    text::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    std::puts("PASS: windows renderer hit test + smoke");
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
#ifdef __APPLE__
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
    test_renderer_flush_empty();
    std::puts("\nAll native tests passed.");
#elif defined(_WIN32)
    test_windows_text_measure_basic();
    test_windows_text_measure_proportional();
    test_windows_text_measure_mono_fixed_width();
    test_windows_text_measure_unicode();
    test_windows_text_build_atlas();
    test_windows_text_build_atlas_scale_preserves_bounds();
    test_windows_text_build_atlas_empty();
    test_renderer_flush_empty();
    test_windows_renderer_hit_test_and_smoke();
    test_windows_text_field_key_dispatch();
    std::puts("\nAll Windows native tests passed.");
#else
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

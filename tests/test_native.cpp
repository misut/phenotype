// Native backend tests — CoreText text measurement/atlas + Metal safety.
// CoreText tests run on any macOS (no GPU needed).
// Metal tests require a Metal device (SKIP if unavailable).

#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

import phenotype.native;

using namespace phenotype::native;

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
// Metal renderer safety tests
// ============================================================

static void test_renderer_flush_empty() {
    // flush with empty buffer should not crash
    unsigned char buf[4] = {};
    renderer::flush(buf, 0);
    std::puts("PASS: renderer flush empty");
}

#else // !__APPLE__

static void skip_all() {
    std::puts("SKIP: all native tests (not macOS)");
}

#endif // __APPLE__

int main() {
#ifdef __APPLE__
    test_text_measure_basic();
    test_text_measure_proportional();
    test_text_measure_mono_fixed_width();
    test_text_measure_empty();
    test_text_measure_unicode();
    test_text_build_atlas();
    test_text_build_atlas_crops_padding();
    test_text_build_atlas_scale_preserves_logical_bounds();
    test_text_build_atlas_respects_line_box();
    test_text_build_atlas_keeps_line_box_stable();
    test_text_build_atlas_empty();
    test_text_init_shutdown_cycle();
    test_renderer_flush_empty();
    std::puts("\nAll native tests passed.");
#else
    skip_all();
#endif
    return 0;
}

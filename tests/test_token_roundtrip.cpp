// Round-trip consumer for the JSON artifact produced by phenotype-web's
// `themeToPhenotypeJson` snapshot test (PR #6).
//
// The literal below MUST stay byte-identical with
// `tests/fixtures/themes/default.theme.json`. That file in turn mirrors
// `phenotype-web/src/theme/__snapshots__/default.theme.json` and is the
// human-visible artifact of the M3 token pipeline. The literal lets
// every target (native + wasi) verify the round-trip without depending
// on filesystem preopens, which exon's wasmtime runner does not grant.
//
// Update flow when phenotype-web's snapshot changes:
//   1. Pull the new snapshot from phenotype-web.
//   2. Overwrite `tests/fixtures/themes/default.theme.json` with it.
//   3. Update the raw-string literal below to the same content.
//   PR review will surface both edits in the same diff.

#include <cassert>
#include <cstdio>
#include <string>

import phenotype;

using namespace phenotype;

#if defined(__wasi__)
// wasi has no JS shim host. Stub the imports phenotype expects so the
// module can instantiate inside wasmtime; this test never actually
// hits the renderer or input handlers, so the stubs are no-ops.
extern "C" {
    extern unsigned char phenotype_cmd_buf[];
    extern unsigned int phenotype_cmd_len;
    void phenotype_flush() {}
    float phenotype_measure_text(float fs, unsigned int, char const*, unsigned int len) {
        return static_cast<float>(len) * fs * 0.6f;
    }
    float phenotype_get_canvas_width() { return 800.0f; }
    float phenotype_get_canvas_height() { return 600.0f; }
    void phenotype_open_url(char const*, unsigned int) {}
}
#endif

namespace {

constexpr char const* DEFAULT_THEME_JSON = R"json({
  "background": {
    "r": 250,
    "g": 250,
    "b": 250,
    "a": 255
  },
  "foreground": {
    "r": 26,
    "g": 26,
    "b": 26,
    "a": 255
  },
  "accent": {
    "r": 10,
    "g": 186,
    "b": 181,
    "a": 255
  },
  "accent_strong": {
    "r": 8,
    "g": 128,
    "b": 124,
    "a": 255
  },
  "muted": {
    "r": 107,
    "g": 114,
    "b": 128,
    "a": 255
  },
  "border": {
    "r": 229,
    "g": 231,
    "b": 235,
    "a": 255
  },
  "surface": {
    "r": 255,
    "g": 255,
    "b": 255,
    "a": 255
  },
  "code_bg": {
    "r": 243,
    "g": 244,
    "b": 246,
    "a": 255
  },
  "hero_bg": {
    "r": 244,
    "g": 244,
    "b": 245,
    "a": 255
  },
  "hero_fg": {
    "r": 24,
    "g": 24,
    "b": 27,
    "a": 255
  },
  "hero_muted": {
    "r": 113,
    "g": 113,
    "b": 122,
    "a": 255
  },
  "transparent": {
    "r": 0,
    "g": 0,
    "b": 0,
    "a": 0
  },
  "state_hover_bg": {
    "r": 229,
    "g": 231,
    "b": 235,
    "a": 255
  },
  "state_hover_fg": {
    "r": 26,
    "g": 26,
    "b": 26,
    "a": 255
  },
  "state_active_bg": {
    "r": 8,
    "g": 128,
    "b": 124,
    "a": 255
  },
  "state_active_fg": {
    "r": 255,
    "g": 255,
    "b": 255,
    "a": 255
  },
  "state_disabled_bg": {
    "r": 243,
    "g": 244,
    "b": 246,
    "a": 255
  },
  "state_disabled_fg": {
    "r": 156,
    "g": 163,
    "b": 175,
    "a": 255
  },
  "state_disabled_border": {
    "r": 229,
    "g": 231,
    "b": 235,
    "a": 255
  },
  "state_error_bg": {
    "r": 254,
    "g": 242,
    "b": 242,
    "a": 255
  },
  "state_error_fg": {
    "r": 185,
    "g": 28,
    "b": 28,
    "a": 255
  },
  "state_error_border": {
    "r": 220,
    "g": 38,
    "b": 38,
    "a": 255
  },
  "state_focus_ring": {
    "r": 10,
    "g": 186,
    "b": 181,
    "a": 255
  },
  "semantic_success_bg": {
    "r": 236,
    "g": 253,
    "b": 245,
    "a": 255
  },
  "semantic_success_fg": {
    "r": 4,
    "g": 120,
    "b": 87,
    "a": 255
  },
  "semantic_success_border": {
    "r": 16,
    "g": 185,
    "b": 129,
    "a": 255
  },
  "semantic_warning_bg": {
    "r": 255,
    "g": 251,
    "b": 235,
    "a": 255
  },
  "semantic_warning_fg": {
    "r": 180,
    "g": 83,
    "b": 9,
    "a": 255
  },
  "semantic_warning_border": {
    "r": 245,
    "g": 158,
    "b": 11,
    "a": 255
  },
  "semantic_info_bg": {
    "r": 239,
    "g": 246,
    "b": 255,
    "a": 255
  },
  "semantic_info_fg": {
    "r": 29,
    "g": 78,
    "b": 216,
    "a": 255
  },
  "semantic_info_border": {
    "r": 59,
    "g": 130,
    "b": 246,
    "a": 255
  },
  "semantic_error_bg": {
    "r": 254,
    "g": 242,
    "b": 242,
    "a": 255
  },
  "semantic_error_fg": {
    "r": 185,
    "g": 28,
    "b": 28,
    "a": 255
  },
  "semantic_error_border": {
    "r": 220,
    "g": 38,
    "b": 38,
    "a": 255
  },
  "body_font_size": 16,
  "heading_font_size": 22.4,
  "hero_title_size": 40,
  "hero_subtitle_size": 18.4,
  "code_font_size": 14.4,
  "small_font_size": 14.4,
  "max_content_width": 720,
  "radius_sm": 2,
  "radius_md": 3,
  "radius_lg": 4,
  "radius_full": 9999,
  "space_xs": 4,
  "space_sm": 8,
  "space_md": 12,
  "space_lg": 16,
  "space_xl": 24,
  "space_2xl": 32,
  "state_focus_ring_width": 2,
  "line_height_ratio": 1.6
}
)json";

} // namespace

void test_phenotype_web_default_theme_roundtrip() {
    auto parsed = theme_from_json(std::string{DEFAULT_THEME_JSON});
    assert(parsed.has_value());

    Theme const& t = *parsed;

    // Base colors that match phenotype's own M0-3 default palette.
    assert(t.background.r == 250 && t.background.g == 250 && t.background.b == 250);
    assert(t.accent.r == 10 && t.accent.g == 186 && t.accent.b == 181); // Tiffany
    assert(t.accent_strong.r == 8 && t.accent_strong.g == 128 && t.accent_strong.b == 124);
    assert(t.surface.r == 255 && t.surface.g == 255 && t.surface.b == 255);
    assert(t.transparent.a == 0);

    // phenotype-web ships smaller "angular" radii than phenotype's
    // own defaults — the JSON must override them rather than fall
    // back to phenotype's defaults (4 / 6 / 8).
    assert(t.radius_sm == 2.0f);
    assert(t.radius_md == 3.0f);
    assert(t.radius_lg == 4.0f);
    assert(t.radius_full == 9999.0f);

    // Spacing scale: phenotype-web matches phenotype's defaults
    // exactly here, so the assertion mostly guards against silently
    // dropped fields.
    assert(t.space_xs == 4.0f);
    assert(t.space_sm == 8.0f);
    assert(t.space_md == 12.0f);
    assert(t.space_lg == 16.0f);
    assert(t.space_xl == 24.0f);
    assert(t.space_2xl == 32.0f);

    // State tokens.
    assert(t.state_hover_bg.r == 229 && t.state_hover_bg.g == 231 && t.state_hover_bg.b == 235);
    assert(t.state_active_bg.r == 8 && t.state_active_bg.g == 128 && t.state_active_bg.b == 124);
    assert(t.state_active_fg.r == 255 && t.state_active_fg.a == 255);
    assert(t.state_disabled_fg.r == 156);
    assert(t.state_focus_ring.r == 10 && t.state_focus_ring.g == 186);
    assert(t.state_focus_ring_width == 2.0f);
    assert(t.state_error_border.r == 220 && t.state_error_border.g == 38);

    // Semantic colors (no widget consumer yet, so this guards the
    // serialization edge of the pipeline only).
    assert(t.semantic_success_bg.r == 236 && t.semantic_success_bg.g == 253);
    assert(t.semantic_warning_fg.r == 180 && t.semantic_warning_fg.g == 83);
    assert(t.semantic_info_border.r == 59 && t.semantic_info_border.b == 246);
    assert(t.semantic_error_fg.r == 185 && t.semantic_error_fg.b == 28);

    // Typography + layout untouched by phenotype-web's default theme.
    assert(t.body_font_size == 16.0f);
    assert(t.line_height_ratio > 1.59f && t.line_height_ratio < 1.61f);
    assert(t.max_content_width == 720.0f);

    std::puts("PASS: phenotype-web default theme round-trip");
}

int main() {
    test_phenotype_web_default_theme_roundtrip();
    return 0;
}

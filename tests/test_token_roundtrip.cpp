// Round-trip consumer for the JSON artifacts produced by phenotype-web's
// `themeToPhenotypeJson` snapshot tests (PR #6, PR #9).
//
// Each raw-string literal below MUST stay byte-equivalent with the
// matching `tests/fixtures/themes/<theme>.theme.json` file. Those
// files mirror `phenotype-web/src/theme/__snapshots__/<theme>.theme.json`
// and are the human-visible artifacts of the M3 token pipeline. The
// literals let every target (native + wasi) verify the round-trip
// without depending on filesystem preopens, which exon's wasmtime
// runner does not grant.
//
// Note: the literals here are the same JSON content as the fixture
// files but emitted in compact form (one row per Color / scalar) to
// keep this test file readable. The `test_token_vocabulary_parity`
// case is what actually compares key sets to phenotype's Theme; the
// shape of the JSON literal does not have to match the fixture file
// byte-for-byte, only the key set and the values.
//
// Update flow when a phenotype-web snapshot changes:
//   1. Pull the new snapshot(s) from phenotype-web.
//   2. Overwrite `tests/fixtures/themes/<theme>.theme.json` with them.
//   3. Update the raw-string literal(s) below to the same values
//      (compact form is fine — the parity guard checks key set, the
//      detail tests pin specific values).
//   PR review surfaces all three edits in the same diff.

#include <cassert>
#include <cstdio>
#include <set>
#include <string>
#include <utility>
#include <vector>

import phenotype;
import json;
import cppx.reflect;

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
    "b": 183,
    "a": 255
  },
  "accent_strong": {
    "r": 8,
    "g": 128,
    "b": 126,
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
    "b": 126,
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
    "b": 183,
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
  "radius_xs": 3,
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
  "space_3xl": 48,
  "state_focus_ring_width": 2,
  "line_height_ratio": 1.6
}
)json";

constexpr char const* DARK_THEME_JSON = R"json({
  "background": {
    "r": 24,
    "g": 24,
    "b": 27,
    "a": 255
  },
  "foreground": {
    "r": 250,
    "g": 250,
    "b": 250,
    "a": 255
  },
  "accent": {
    "r": 139,
    "g": 26,
    "b": 43,
    "a": 255
  },
  "accent_strong": {
    "r": 94,
    "g": 16,
    "b": 29,
    "a": 255
  },
  "muted": {
    "r": 161,
    "g": 161,
    "b": 170,
    "a": 255
  },
  "border": {
    "r": 63,
    "g": 63,
    "b": 70,
    "a": 255
  },
  "surface": {
    "r": 39,
    "g": 39,
    "b": 42,
    "a": 255
  },
  "code_bg": {
    "r": 39,
    "g": 39,
    "b": 42,
    "a": 255
  },
  "hero_bg": {
    "r": 39,
    "g": 39,
    "b": 42,
    "a": 255
  },
  "hero_fg": {
    "r": 250,
    "g": 250,
    "b": 250,
    "a": 255
  },
  "hero_muted": {
    "r": 161,
    "g": 161,
    "b": 170,
    "a": 255
  },
  "transparent": {
    "r": 0,
    "g": 0,
    "b": 0,
    "a": 0
  },
  "state_hover_bg": {
    "r": 63,
    "g": 63,
    "b": 70,
    "a": 255
  },
  "state_hover_fg": {
    "r": 250,
    "g": 250,
    "b": 250,
    "a": 255
  },
  "state_active_bg": {
    "r": 94,
    "g": 16,
    "b": 29,
    "a": 255
  },
  "state_active_fg": {
    "r": 255,
    "g": 255,
    "b": 255,
    "a": 255
  },
  "state_disabled_bg": {
    "r": 63,
    "g": 63,
    "b": 70,
    "a": 255
  },
  "state_disabled_fg": {
    "r": 113,
    "g": 113,
    "b": 122,
    "a": 255
  },
  "state_disabled_border": {
    "r": 63,
    "g": 63,
    "b": 70,
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
    "r": 139,
    "g": 26,
    "b": 43,
    "a": 255
  },
  "semantic_success_bg": {
    "r": 5,
    "g": 46,
    "b": 31,
    "a": 255
  },
  "semantic_success_fg": {
    "r": 110,
    "g": 231,
    "b": 183,
    "a": 255
  },
  "semantic_success_border": {
    "r": 16,
    "g": 185,
    "b": 129,
    "a": 255
  },
  "semantic_warning_bg": {
    "r": 59,
    "g": 36,
    "b": 9,
    "a": 255
  },
  "semantic_warning_fg": {
    "r": 252,
    "g": 211,
    "b": 77,
    "a": 255
  },
  "semantic_warning_border": {
    "r": 245,
    "g": 158,
    "b": 11,
    "a": 255
  },
  "semantic_info_bg": {
    "r": 12,
    "g": 30,
    "b": 61,
    "a": 255
  },
  "semantic_info_fg": {
    "r": 147,
    "g": 197,
    "b": 253,
    "a": 255
  },
  "semantic_info_border": {
    "r": 59,
    "g": 130,
    "b": 246,
    "a": 255
  },
  "semantic_error_bg": {
    "r": 69,
    "g": 10,
    "b": 10,
    "a": 255
  },
  "semantic_error_fg": {
    "r": 254,
    "g": 202,
    "b": 202,
    "a": 255
  },
  "semantic_error_border": {
    "r": 239,
    "g": 68,
    "b": 68,
    "a": 255
  },
  "body_font_size": 16,
  "heading_font_size": 22.4,
  "hero_title_size": 40,
  "hero_subtitle_size": 18.4,
  "code_font_size": 14.4,
  "small_font_size": 14.4,
  "max_content_width": 720,
  "radius_xs": 3,
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
  "space_3xl": 48,
  "state_focus_ring_width": 2,
  "line_height_ratio": 1.6
}
)json";

constexpr char const* WARM_THEME_JSON = R"json({
  "background": {
    "r": 255,
    "g": 250,
    "b": 243,
    "a": 255
  },
  "foreground": {
    "r": 28,
    "g": 25,
    "b": 23,
    "a": 255
  },
  "accent": {
    "r": 234,
    "g": 88,
    "b": 12,
    "a": 255
  },
  "accent_strong": {
    "r": 194,
    "g": 65,
    "b": 12,
    "a": 255
  },
  "muted": {
    "r": 120,
    "g": 113,
    "b": 108,
    "a": 255
  },
  "border": {
    "r": 231,
    "g": 229,
    "b": 228,
    "a": 255
  },
  "surface": {
    "r": 255,
    "g": 255,
    "b": 255,
    "a": 255
  },
  "code_bg": {
    "r": 245,
    "g": 245,
    "b": 244,
    "a": 255
  },
  "hero_bg": {
    "r": 67,
    "g": 20,
    "b": 7,
    "a": 255
  },
  "hero_fg": {
    "r": 255,
    "g": 247,
    "b": 237,
    "a": 255
  },
  "hero_muted": {
    "r": 253,
    "g": 186,
    "b": 116,
    "a": 255
  },
  "transparent": {
    "r": 0,
    "g": 0,
    "b": 0,
    "a": 0
  },
  "state_hover_bg": {
    "r": 231,
    "g": 229,
    "b": 228,
    "a": 255
  },
  "state_hover_fg": {
    "r": 28,
    "g": 25,
    "b": 23,
    "a": 255
  },
  "state_active_bg": {
    "r": 194,
    "g": 65,
    "b": 12,
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
    "r": 234,
    "g": 88,
    "b": 12,
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
  "radius_xs": 3,
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
  "space_3xl": 48,
  "state_focus_ring_width": 2,
  "line_height_ratio": 1.6
}
)json";

constexpr char const* DENSE_THEME_JSON = R"json({
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
    "b": 183,
    "a": 255
  },
  "accent_strong": {
    "r": 8,
    "g": 128,
    "b": 126,
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
    "b": 126,
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
    "b": 183,
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
  "body_font_size": 14,
  "heading_font_size": 19.6,
  "hero_title_size": 32,
  "hero_subtitle_size": 16,
  "code_font_size": 12.6,
  "small_font_size": 12.6,
  "max_content_width": 720,
  "radius_xs": 3,
  "radius_sm": 3,
  "radius_md": 4,
  "radius_lg": 6,
  "radius_full": 9999,
  "space_xs": 2,
  "space_sm": 4,
  "space_md": 8,
  "space_lg": 12,
  "space_xl": 16,
  "space_2xl": 24,
  "space_3xl": 48,
  "state_focus_ring_width": 2,
  "line_height_ratio": 1.4
}
)json";

struct ThemeFixture {
    char const* name;
    char const* json;
};

constexpr ThemeFixture THEME_FIXTURES[] = {
    {"default", DEFAULT_THEME_JSON},
    {"dark",    DARK_THEME_JSON},
    {"warm",    WARM_THEME_JSON},
    {"dense",   DENSE_THEME_JSON},
};

} // namespace

void test_phenotype_web_default_theme_roundtrip() {
    auto parsed = theme_from_json(std::string{DEFAULT_THEME_JSON});
    assert(parsed.has_value());

    Theme const& t = *parsed;

    // Base colors that match phenotype's own M0-3 default palette.
    assert(t.background.r == 250 && t.background.g == 250 && t.background.b == 250);
    assert(t.accent.r == 10 && t.accent.g == 186 && t.accent.b == 183); // Tiffany
    assert(t.accent_strong.r == 8 && t.accent_strong.g == 128 && t.accent_strong.b == 126);
    assert(t.surface.r == 255 && t.surface.g == 255 && t.surface.b == 255);
    assert(t.transparent.a == 0);

    // phenotype-web's "angular" radius defaults — radius_xs (3) and
    // radius_full (9999) match phenotype's defaults exactly; the
    // sm / md / lg rungs are smaller (2/3/4 vs phenotype's earlier
    // 4/6/8) and rely on the JSON override path.
    assert(t.radius_xs == 3.0f);
    assert(t.radius_sm == 2.0f);
    assert(t.radius_md == 3.0f);
    assert(t.radius_lg == 4.0f);
    assert(t.radius_full == 9999.0f);

    // Spacing scale: phenotype-web mirrors phenotype's defaults
    // exactly across all seven rungs, so the assertions mostly guard
    // against silently dropped fields.
    assert(t.space_xs == 4.0f);
    assert(t.space_sm == 8.0f);
    assert(t.space_md == 12.0f);
    assert(t.space_lg == 16.0f);
    assert(t.space_xl == 24.0f);
    assert(t.space_2xl == 32.0f);
    assert(t.space_3xl == 48.0f);

    // State tokens.
    assert(t.state_hover_bg.r == 229 && t.state_hover_bg.g == 231 && t.state_hover_bg.b == 235);
    assert(t.state_active_bg.r == 8 && t.state_active_bg.g == 128 && t.state_active_bg.b == 126);
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

void test_phenotype_web_dark_theme_roundtrip() {
    auto parsed = theme_from_json(std::string{DARK_THEME_JSON});
    assert(parsed.has_value());
    Theme const& t = *parsed;

    // Crimson accent + zinc-900 chrome.
    assert(t.background.r == 24 && t.background.g == 24 && t.background.b == 27);
    assert(t.surface.r == 39 && t.surface.g == 39 && t.surface.b == 42);
    assert(t.foreground.r == 250);
    assert(t.accent.r == 139 && t.accent.g == 26 && t.accent.b == 43);
    assert(t.accent_strong.r == 94 && t.accent_strong.g == 16 && t.accent_strong.b == 29);
    assert(t.border.r == 63 && t.border.g == 63 && t.border.b == 70);

    // var() chains in default.css resolve against dark base values.
    assert(t.state_hover_bg.r == 63);             // dark border
    assert(t.state_hover_fg.r == 250);            // dark foreground
    assert(t.state_active_bg.r == 94);            // dark accent_strong
    assert(t.state_focus_ring.r == 139);          // dark accent

    // dark-only semantic palette.
    assert(t.semantic_success_bg.r == 5 && t.semantic_success_fg.r == 110);
    assert(t.semantic_error_bg.r == 69 && t.semantic_error_fg.r == 254);

    // Typography untouched.
    assert(t.body_font_size == 16.0f);
    assert(t.line_height_ratio > 1.59f && t.line_height_ratio < 1.61f);

    std::puts("PASS: phenotype-web dark theme round-trip");
}

void test_phenotype_web_warm_theme_roundtrip() {
    auto parsed = theme_from_json(std::string{WARM_THEME_JSON});
    assert(parsed.has_value());
    Theme const& t = *parsed;

    // Terracotta accent + warmed neutrals.
    assert(t.background.r == 255 && t.background.g == 250 && t.background.b == 243);
    assert(t.foreground.r == 28 && t.foreground.g == 25 && t.foreground.b == 23);
    assert(t.accent.r == 234 && t.accent.g == 88 && t.accent.b == 12);
    assert(t.accent_strong.r == 194 && t.accent_strong.g == 65 && t.accent_strong.b == 12);
    assert(t.hero_bg.r == 67 && t.hero_bg.g == 20);
    assert(t.surface.r == 255 && t.surface.g == 255 && t.surface.b == 255);

    // var() chains resolve against warm base values.
    assert(t.state_hover_bg.r == 231);            // warm border
    assert(t.state_hover_fg.r == 28);             // warm foreground
    assert(t.state_active_bg.r == 194);           // warm accent_strong
    assert(t.state_focus_ring.r == 234);          // warm accent

    // semantic palette inherited from default.
    assert(t.semantic_success_bg.r == 236);

    std::puts("PASS: phenotype-web warm theme round-trip");
}

void test_phenotype_web_dense_theme_roundtrip() {
    auto parsed = theme_from_json(std::string{DENSE_THEME_JSON});
    assert(parsed.has_value());
    Theme const& t = *parsed;

    // Colors unchanged from default.
    assert(t.background.r == 250 && t.background.g == 250 && t.background.b == 250);
    assert(t.accent.r == 10 && t.accent.g == 186 && t.accent.b == 183);

    // Tightened typography + spacing.
    assert(t.body_font_size == 14.0f);
    assert(t.heading_font_size > 19.5f && t.heading_font_size < 19.7f);
    assert(t.hero_title_size == 32.0f);
    assert(t.line_height_ratio > 1.39f && t.line_height_ratio < 1.41f);

    assert(t.space_xs == 2.0f);
    assert(t.space_sm == 4.0f);
    assert(t.space_md == 8.0f);
    assert(t.space_2xl == 24.0f);
    // space_3xl unchanged from default (dense did not override).
    assert(t.space_3xl == 48.0f);

    assert(t.radius_sm == 3.0f);
    assert(t.radius_md == 4.0f);
    assert(t.radius_lg == 6.0f);

    std::puts("PASS: phenotype-web dense theme round-trip");
}

void test_token_vocabulary_parity() {
    // partial-mode deserialization would silently swallow drift between
    // phenotype's Theme and any phenotype-web fixture (extra Theme
    // fields fall back to C++ defaults, unknown JSON keys are
    // ignored). This guard catches both directions explicitly across
    // every theme.

    std::set<std::string> theme_fields;
    [&]<std::size_t... Is>(std::index_sequence<Is...>) {
        (theme_fields.insert(std::string(cppx::reflect::name_of<Theme, Is>())), ...);
    }(std::make_index_sequence<cppx::reflect::tuple_size_v<Theme>>{});

    bool any_failed = false;
    for (auto const& fixture : THEME_FIXTURES) {
        auto value = json::parse(std::string{fixture.json});
        assert(value.is_object());

        std::set<std::string> fixture_keys;
        for (auto const& [name, _] : value.as_object()) {
            fixture_keys.insert(name);
        }

        std::vector<std::string> missing_in_fixture;
        std::vector<std::string> unknown_to_phenotype;
        for (auto const& field : theme_fields) {
            if (!fixture_keys.contains(field)) missing_in_fixture.push_back(field);
        }
        for (auto const& key : fixture_keys) {
            if (!theme_fields.contains(key)) unknown_to_phenotype.push_back(key);
        }

        if (!missing_in_fixture.empty() || !unknown_to_phenotype.empty()) {
            any_failed = true;
            std::printf("Theme/fixture parity FAILED for [%s]:\n", fixture.name);
            for (auto const& f : missing_in_fixture) {
                std::printf("  - missing from fixture: %s\n", f.c_str());
            }
            for (auto const& k : unknown_to_phenotype) {
                std::printf("  - unknown to phenotype: %s\n", k.c_str());
            }
        }
    }
    assert(!any_failed && "phenotype Theme and one or more phenotype-web fixtures have drifted");

    std::printf("PASS: token vocabulary parity across all 4 themes (%zu fields)\n",
                theme_fields.size());
}

int main() {
    test_phenotype_web_default_theme_roundtrip();
    test_phenotype_web_dark_theme_roundtrip();
    test_phenotype_web_warm_theme_roundtrip();
    test_phenotype_web_dense_theme_roundtrip();
    test_token_vocabulary_parity();
    return 0;
}

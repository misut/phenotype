module;
#include <cstdint>
#include <cstring>
#include <string>
export module phenotype.paint;

import phenotype.types;
import phenotype.state;
import phenotype.diag;

// Host imports
extern "C" __attribute__((import_module("phenotype"), import_name("flush")))
void phenotype_flush(void);

extern "C" __attribute__((import_module("phenotype"), import_name("measure_text")))
float phenotype_measure_text(float font_size, unsigned int mono,
                             char const* text, unsigned int len);

// ============================================================
// Command buffer — shared memory between C++ and JS
// ============================================================

constexpr unsigned int BUF_SIZE = 65536; // 64KB

extern "C" {
    alignas(4) unsigned char phenotype_cmd_buf[BUF_SIZE];
    unsigned int phenotype_cmd_len = 0;

    __attribute__((export_name("phenotype_get_cmd_buf")))
    unsigned char* phenotype_get_cmd_buf(void) { return phenotype_cmd_buf; }

    __attribute__((export_name("phenotype_get_cmd_len")))
    unsigned int phenotype_get_cmd_len(void) { return phenotype_cmd_len; }
}

namespace phenotype::detail {

inline void ensure(unsigned int needed) {
    if (phenotype_cmd_len + needed > BUF_SIZE) {
        phenotype_flush();
        phenotype_cmd_len = 0;
    }
}

inline void write_u32(unsigned int value) {
    auto* p = reinterpret_cast<unsigned int*>(&phenotype_cmd_buf[phenotype_cmd_len]);
    *p = value;
    phenotype_cmd_len += 4;
}

inline void write_f32(float value) {
    unsigned int bits;
    std::memcpy(&bits, &value, 4);
    write_u32(bits);
}

inline void write_bytes(char const* data, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        phenotype_cmd_buf[phenotype_cmd_len++] = static_cast<unsigned char>(data[i]);
    while (phenotype_cmd_len % 4 != 0)
        phenotype_cmd_buf[phenotype_cmd_len++] = 0;
}

inline unsigned int padded(unsigned int len) {
    return (len + 3) & ~3u;
}

// FNV-1a 64-bit. Used by phenotype::flush_if_changed() to detect
// frames whose cmd buffer is byte-identical to the previous frame
// (caret blink, idle repaints, etc.) and skip the JS↔WASM flush
// trampoline. Standard, predictable, microseconds for the typical
// 4-8 KB cmd buffer; collision probability ~1e-15.
inline std::uint64_t fnv1a_64(unsigned char const* data, unsigned int len) noexcept {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned int i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

} // namespace phenotype::detail

export namespace phenotype {

// Draw command emitters

inline void emit_clear(Color c) {
    detail::ensure(8);
    detail::write_u32(static_cast<unsigned int>(Cmd::Clear));
    detail::write_u32(c.packed());
}

inline void emit_fill_rect(float x, float y, float w, float h, Color c) {
    detail::ensure(24);
    detail::write_u32(static_cast<unsigned int>(Cmd::FillRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_u32(c.packed());
}

inline void emit_stroke_rect(float x, float y, float w, float h, float line_width, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::StrokeRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_f32(line_width);
    detail::write_u32(c.packed());
}

inline void emit_round_rect(float x, float y, float w, float h, float radius, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::RoundRect));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_f32(radius);
    detail::write_u32(c.packed());
}

inline void emit_draw_text(float x, float y, float font_size, unsigned int mono,
                           Color c, char const* text, unsigned int len) {
    detail::ensure(28 + detail::padded(len));
    detail::write_u32(static_cast<unsigned int>(Cmd::DrawText));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(font_size);
    detail::write_u32(mono);
    detail::write_u32(c.packed());
    detail::write_u32(len);
    detail::write_bytes(text, len);
}

inline void emit_draw_image(float x, float y, float w, float h,
                            char const* url, unsigned int len) {
    detail::ensure(24 + detail::padded(len));
    detail::write_u32(static_cast<unsigned int>(Cmd::DrawImage));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_u32(len);
    detail::write_bytes(url, len);
}

inline void emit_draw_line(float x1, float y1, float x2, float y2, float thickness, Color c) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::DrawLine));
    detail::write_f32(x1);
    detail::write_f32(y1);
    detail::write_f32(x2);
    detail::write_f32(y2);
    detail::write_f32(thickness);
    detail::write_u32(c.packed());
}

inline void emit_hit_region(float x, float y, float w, float h,
                            unsigned int callback_id, unsigned int cursor_type) {
    detail::ensure(28);
    detail::write_u32(static_cast<unsigned int>(Cmd::HitRegion));
    detail::write_f32(x);
    detail::write_f32(y);
    detail::write_f32(w);
    detail::write_f32(h);
    detail::write_u32(callback_id);
    detail::write_u32(cursor_type);
}

inline void flush() {
    if (phenotype_cmd_len == 0) return;
    phenotype_flush();
    phenotype_cmd_len = 0;
}

// flush_if_changed — frame-skip-aware version of flush() that hashes
// the current cmd buffer and compares it to the previous frame. If the
// bytes are identical, the buffer is discarded and no JS↔WASM round
// trip happens — the previous frame's pixels stay on screen, which
// is the correct visible result. Used by both the runner (full-rebuild
// path) and phenotype_repaint (re-paint-only path) so caret blinks,
// idle repaints, and any future "useless rebuild" trigger collapse to
// a hash + return rather than a full GPU upload + draw.
//
// Diag counters: frames_skipped on the skip branch, flush_calls on
// the flush branch. The headline KPI is
// frames_skipped / (frames_skipped + flush_calls).
//
// Animation safety note: when an animated widget is added in a
// future PR, its cmd buffer bytes will differ on every tick (because
// the animated values are time-derived state baked into the draw
// commands), so the hash will mismatch and frame skip will not
// erroneously suppress motion.
inline void flush_if_changed() {
    if (phenotype_cmd_len == 0) return;
    auto hash = detail::fnv1a_64(phenotype_cmd_buf, phenotype_cmd_len);
    auto& app = detail::g_app;
    if (hash == app.last_paint_hash) {
        // Identical to previous frame — discard the buffer and skip.
        phenotype_cmd_len = 0;
        metrics::inst::frames_skipped.add();
        return;
    }
    app.last_paint_hash = hash;
    flush();
    metrics::inst::flush_calls.add();
}

} // namespace phenotype

// ============================================================
// paint_node — walk layout tree, emit draw commands
// ============================================================

export namespace phenotype::detail {

inline void paint_node(NodeHandle node_h, float ox, float oy, float scroll_y,
                       float vp_height) {
    auto& node = node_at(node_h);
    float ax = ox + node.x;
    float ay = oy + node.y;

    // Viewport culling (leaf nodes only — containers delegate to children)
    if (node.children.empty()) {
        if (ay + node.height < scroll_y || ay > scroll_y + vp_height) return;
    }

    float draw_y = ay - scroll_y;

    // Hover/focus state. Focus is gated on `focusable` so non-focusable
    // click targets (e.g. the label leaf inside widget::checkbox/radio,
    // which shares its callback_id with the indicator box for click
    // dispatch) never satisfy the focus-ring branch even when the JS
    // shim's click handler set focused_id to their shared id.
    bool is_hovered = (node.callback_id != 0xFFFFFFFF &&
                       node.callback_id == g_app.hovered_id);
    bool is_focused = (node.callback_id != 0xFFFFFFFF &&
                       node.callback_id == g_app.focused_id &&
                       node.focusable);

    // Background (with hover override)
    Color bg = (is_hovered && node.hover_background.a > 0)
        ? node.hover_background : node.background;
    if (bg.a > 0) {
        if (node.border_radius > 0)
            emit_round_rect(ax, draw_y, node.width, node.height,
                            node.border_radius, bg);
        else
            emit_fill_rect(ax, draw_y, node.width, node.height, bg);
    }

    // Decoration — small white glyph drawn on top of the background.
    // Used by widget::checkbox and widget::radio to render the active
    // state without a font glyph or a new opcode. The geometry is
    // empirical, scaled by node.width so it stays balanced if the
    // 16x16 indicator size ever changes.
    if (node.decoration != Decoration::None) {
        Color white = {255, 255, 255, 255};
        if (node.decoration == Decoration::Check) {
            // V-shaped checkmark from two short diagonal strokes. The
            // elbow sits slightly below center; the long arm runs up
            // and to the right.
            float cx = ax + node.width * 0.5f;
            float cy = draw_y + node.height * 0.5f;
            float u  = node.width;
            emit_draw_line(cx - u * 0.25f, cy + u * 0.02f,
                           cx - u * 0.05f, cy + u * 0.18f,
                           2.0f, white);
            emit_draw_line(cx - u * 0.05f, cy + u * 0.18f,
                           cx + u * 0.28f, cy - u * 0.18f,
                           2.0f, white);
        } else { // Decoration::Dot
            float dot = node.width * 0.4f;
            float dx  = ax + (node.width  - dot) * 0.5f;
            float dy  = draw_y + (node.height - dot) * 0.5f;
            emit_round_rect(dx, dy, dot, dot, dot * 0.5f, white);
        }
    }

    // Border (accent color when focused)
    Color bc = is_focused ? g_app.theme.accent : node.border_color;
    float bw = is_focused ? 2.0f : node.border_width;
    if (bw > 0 && bc.a > 0) {
        emit_stroke_rect(ax, draw_y, node.width, node.height, bw, bc);
    }

    // Text — use cached text_lines from layout pass.
    //
    // Skip when this node is the focused text input: the JS shim is
    // rendering an HTML <input> overlay over it so that the OS IME's
    // inline composition (Korean / Japanese / Chinese / etc.) is
    // visible to the user. The canvas still paints the field's
    // background, border, and focus ring; the overlay handles the
    // text glyphs and the caret natively.
    bool html_overlay_active = is_focused && node.is_input;
    Color tc = (is_hovered && node.hover_text_color.a > 0)
        ? node.hover_text_color : node.text_color;
    if (!html_overlay_active && !node.text_lines.empty()) {
        float line_height = node.font_size * g_app.theme.line_height_ratio;
        float inner_width = node.width - node.style.padding[1] - node.style.padding[3];
        float ty = draw_y + node.style.padding[0];
        for (auto const& line : node.text_lines) {
            if (!line.empty()) {
                float line_w = phenotype_measure_text(
                    node.font_size, node.mono ? 1 : 0,
                    line.c_str(), static_cast<unsigned int>(line.size()));
                float tx = ax + node.style.padding[3];
                if (node.style.text_align == TextAlign::Center)
                    tx += (inner_width - line_w) / 2;
                else if (node.style.text_align == TextAlign::End)
                    tx += inner_width - line_w;
                emit_draw_text(tx, ty, node.font_size, node.mono ? 1 : 0,
                               tc, line.c_str(),
                               static_cast<unsigned int>(line.size()));
            }
            ty += line_height;
        }
    }

    // Caret for focused input — handled by the HTML overlay now, so
    // the canvas no longer paints it. Kept here as a comment because
    // the focus-ring border immediately above is still painted by
    // the canvas, and the next reader will want to know why the
    // caret block is missing.

    // Image leaf — emit DrawImage with the world-space rect and the
    // URL bytes. The JS shim looks up the URL in its persistent image
    // cache, lazy-loads on first sight, and renders a placeholder
    // grey rectangle until the load completes (and triggers a repaint).
    if (!node.image_url.empty()) {
        emit_draw_image(ax, draw_y, node.width, node.height,
                        node.image_url.c_str(),
                        static_cast<unsigned int>(node.image_url.size()));
    }

    // Collect focusable IDs (for Tab navigation). Gated on `focusable`
    // so non-focusable click targets (label leaf of checkbox/radio,
    // which shares its callback_id with the indicator) are not visited
    // by Tab — the indicator alone owns the focus slot.
    if (node.callback_id != 0xFFFFFFFF && node.focusable)
        g_app.focusable_ids.push_back(node.callback_id);

    // Hit region — emit in world-space (pre-scroll). The JS hit-test
    // adds scrollY back when comparing against client coordinates, so
    // both sides end up in world-space and the click area tracks the
    // visual button under any scroll offset.
    if (node.callback_id != 0xFFFFFFFF) {
        emit_hit_region(ax, ay, node.width, node.height,
                        node.callback_id, node.cursor_type);
    }

    // Children
    for (auto child_h : node.children)
        paint_node(child_h, ax, ay, scroll_y, vp_height);
}

} // namespace phenotype::detail

module;
#include <cstdint>
#include <cstring>
#include <string>
#ifdef __wasi__
#include "phenotype_host.h"
#endif
export module phenotype.paint;

import phenotype.types;
import phenotype.state;
import phenotype.diag;
import phenotype.host;

// ============================================================
// WASM: global command buffer (shared memory between C++ and JS)
// ============================================================
#ifdef __wasi__
constexpr unsigned int BUF_SIZE = 65536;

extern "C" {
    alignas(4) unsigned char phenotype_cmd_buf[BUF_SIZE];
    unsigned int phenotype_cmd_len = 0;

    __attribute__((export_name("phenotype_get_cmd_buf")))
    unsigned char* phenotype_get_cmd_buf(void) { return phenotype_cmd_buf; }
    __attribute__((export_name("phenotype_get_cmd_len")))
    unsigned int phenotype_get_cmd_len(void) { return phenotype_cmd_len; }
}
#endif

namespace phenotype::detail {

// ---- Buffer write helpers (templated on render_backend) ----

template <render_backend R>
void ensure(R& r, unsigned int needed) {
    if (r.buf_len() + needed > r.buf_size()) {
        r.flush();
        r.buf_len() = 0;
    }
}

template <render_backend R>
void write_u32(R& r, unsigned int value) {
    auto* p = reinterpret_cast<unsigned int*>(&r.buf()[r.buf_len()]);
    *p = value;
    r.buf_len() += 4;
}

template <render_backend R>
void write_f32(R& r, float value) {
    unsigned int bits;
    std::memcpy(&bits, &value, 4);
    write_u32(r, bits);
}

template <render_backend R>
void write_bytes(R& r, char const* data, unsigned int len) {
    for (unsigned int i = 0; i < len; ++i)
        r.buf()[r.buf_len()++] = static_cast<unsigned char>(data[i]);
    while (r.buf_len() % 4 != 0)
        r.buf()[r.buf_len()++] = 0;
}

inline unsigned int padded(unsigned int len) {
    return (len + 3) & ~3u;
}

inline std::uint64_t fnv1a_64(unsigned char const* data, unsigned int len) noexcept {
    std::uint64_t h = 0xcbf29ce484222325ULL;
    for (unsigned int i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 0x100000001b3ULL;
    }
    return h;
}

// ---- WASM: host type using global cmd buffer + phenotype_host.h ----
#ifdef __wasi__
struct wasi_paint_host {
    float measure_text(float fs, unsigned int m,
                       char const* t, unsigned int l) const {
        return phenotype_measure_text(fs, m, t, l);
    }
    unsigned char* buf() { return phenotype_cmd_buf; }
    unsigned int& buf_len() { return phenotype_cmd_len; }
    unsigned int buf_size() { return BUF_SIZE; }
    void ensure(unsigned int n) {
        if (phenotype_cmd_len + n > BUF_SIZE) {
            phenotype_flush(); phenotype_cmd_len = 0;
        }
    }
    void flush() {
        if (phenotype_cmd_len > 0) {
            phenotype_flush(); phenotype_cmd_len = 0;
        }
    }
    float canvas_width() const { return phenotype_get_canvas_width(); }
    float canvas_height() const { return phenotype_get_canvas_height(); }
    void open_url(char const* u, unsigned int l) { phenotype_open_url(u, l); }
};
inline wasi_paint_host g_wasi;
#endif

} // namespace phenotype::detail

export namespace phenotype {

// ---- Draw command emitters (templated) ----

template <render_backend R>
void emit_clear(R& r, Color c) {
    detail::ensure(r, 8);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::Clear));
    detail::write_u32(r, c.packed());
}

template <render_backend R>
void emit_fill_rect(R& r, float x, float y, float w, float h, Color c) {
    detail::ensure(r, 24);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::FillRect));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_u32(r, c.packed());
}

template <render_backend R>
void emit_stroke_rect(R& r, float x, float y, float w, float h, float lw, Color c) {
    detail::ensure(r, 28);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::StrokeRect));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_f32(r, lw);
    detail::write_u32(r, c.packed());
}

template <render_backend R>
void emit_round_rect(R& r, float x, float y, float w, float h, float radius, Color c) {
    detail::ensure(r, 28);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::RoundRect));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_f32(r, radius);
    detail::write_u32(r, c.packed());
}

template <render_backend R>
void emit_draw_text(R& r, float x, float y, float font_size, unsigned int mono,
                    Color c, char const* text, unsigned int len) {
    detail::ensure(r, 28 + detail::padded(len));
    detail::write_u32(r, static_cast<unsigned int>(Cmd::DrawText));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, font_size);
    detail::write_u32(r, mono);
    detail::write_u32(r, c.packed());
    detail::write_u32(r, len);
    detail::write_bytes(r, text, len);
}

template <render_backend R>
void emit_draw_image(R& r, float x, float y, float w, float h,
                     char const* url, unsigned int len) {
    detail::ensure(r, 24 + detail::padded(len));
    detail::write_u32(r, static_cast<unsigned int>(Cmd::DrawImage));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_u32(r, len);
    detail::write_bytes(r, url, len);
}

template <render_backend R>
void emit_draw_line(R& r, float x1, float y1, float x2, float y2,
                    float thickness, Color c) {
    detail::ensure(r, 28);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::DrawLine));
    detail::write_f32(r, x1); detail::write_f32(r, y1);
    detail::write_f32(r, x2); detail::write_f32(r, y2);
    detail::write_f32(r, thickness);
    detail::write_u32(r, c.packed());
}

template <render_backend R>
void emit_hit_region(R& r, float x, float y, float w, float h,
                     unsigned int callback_id, unsigned int cursor_type) {
    detail::ensure(r, 28);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::HitRegion));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_u32(r, callback_id);
    detail::write_u32(r, cursor_type);
}

template <render_backend R>
void flush(R& r) {
    if (r.buf_len() == 0) return;
    r.flush();
    r.buf_len() = 0;
}

template <render_backend R>
void flush_if_changed(R& r) {
    if (r.buf_len() == 0) return;
    auto hash = detail::fnv1a_64(r.buf(), r.buf_len());
    auto& app = detail::g_app;
    if (hash == app.last_paint_hash) {
        r.buf_len() = 0;
        metrics::inst::frames_skipped.add();
        return;
    }
    app.last_paint_hash = hash;
    flush(r);
    metrics::inst::flush_calls.add();
}

// ---- WASM non-template overloads ----
#ifdef __wasi__
inline void emit_clear(Color c) { emit_clear(detail::g_wasi, c); }
inline void flush_if_changed() { flush_if_changed(detail::g_wasi); }
#endif

} // namespace phenotype

// ============================================================
// paint_node — walk layout tree, emit draw commands
// ============================================================

export namespace phenotype::detail {

template <render_backend R, text_measurer M>
void paint_node(R& r, M const& measurer, NodeHandle node_h,
                float ox, float oy, float scroll_y, float vp_height) {
    auto& node = node_at(node_h);
    float ax = ox + node.x;
    float ay = oy + node.y;

    if (node.children.empty()) {
        if (ay + node.height < scroll_y || ay > scroll_y + vp_height) return;
    }

    float draw_y = ay - scroll_y;

    bool is_hovered = (node.callback_id != 0xFFFFFFFF &&
                       node.callback_id == g_app.hovered_id);
    bool is_focused = (node.callback_id != 0xFFFFFFFF &&
                       node.callback_id == g_app.focused_id &&
                       node.focusable);

    Color bg = (is_hovered && node.hover_background.a > 0)
        ? node.hover_background : node.background;
    if (bg.a > 0) {
        if (node.border_radius > 0)
            emit_round_rect(r, ax, draw_y, node.width, node.height,
                            node.border_radius, bg);
        else
            emit_fill_rect(r, ax, draw_y, node.width, node.height, bg);
    }

    if (node.decoration != Decoration::None) {
        Color white = {255, 255, 255, 255};
        if (node.decoration == Decoration::Check) {
            float cx = ax + node.width * 0.5f;
            float cy = draw_y + node.height * 0.5f;
            float u  = node.width;
            emit_draw_line(r, cx - u * 0.25f, cy + u * 0.02f,
                           cx - u * 0.05f, cy + u * 0.18f, 2.0f, white);
            emit_draw_line(r, cx - u * 0.05f, cy + u * 0.18f,
                           cx + u * 0.28f, cy - u * 0.18f, 2.0f, white);
        } else {
            float dot = node.width * 0.4f;
            float dx  = ax + (node.width  - dot) * 0.5f;
            float dy  = draw_y + (node.height - dot) * 0.5f;
            emit_round_rect(r, dx, dy, dot, dot, dot * 0.5f, white);
        }
    }

    Color bc = is_focused ? g_app.theme.accent : node.border_color;
    float bw = is_focused ? 2.0f : node.border_width;
    if (bw > 0 && bc.a > 0) {
        emit_stroke_rect(r, ax, draw_y, node.width, node.height, bw, bc);
    }

    bool html_overlay_active = false;
#ifdef __wasi__
    html_overlay_active = is_focused && node.is_input;
#endif
    Color tc = (is_hovered && node.hover_text_color.a > 0)
        ? node.hover_text_color : node.text_color;
    if (!html_overlay_active && !node.text_lines.empty()) {
        float line_height = node.font_size * g_app.theme.line_height_ratio;
        float inner_width = node.width - node.style.padding[1] - node.style.padding[3];
        float ty = draw_y + node.style.padding[0];
        for (auto const& line : node.text_lines) {
            if (!line.empty()) {
                float line_w = measurer.measure_text(
                    node.font_size, node.mono ? 1 : 0,
                    line.c_str(), static_cast<unsigned int>(line.size()));
                float tx = ax + node.style.padding[3];
                if (node.style.text_align == TextAlign::Center)
                    tx += (inner_width - line_w) / 2;
                else if (node.style.text_align == TextAlign::End)
                    tx += inner_width - line_w;
                emit_draw_text(r, tx, ty, node.font_size, node.mono ? 1 : 0,
                               tc, line.c_str(),
                               static_cast<unsigned int>(line.size()));
            }
            ty += line_height;
        }
    }

    if (!node.image_url.empty()) {
        emit_draw_image(r, ax, draw_y, node.width, node.height,
                        node.image_url.c_str(),
                        static_cast<unsigned int>(node.image_url.size()));
    }

    if (node.callback_id != 0xFFFFFFFF && node.focusable)
        g_app.focusable_ids.push_back(node.callback_id);

    if (node.callback_id != 0xFFFFFFFF) {
        emit_hit_region(r, ax, ay, node.width, node.height,
                        node.callback_id, node.cursor_type);
    }

    for (auto child_h : node.children)
        paint_node(r, measurer, child_h, ax, ay, scroll_y, vp_height);
}

} // namespace phenotype::detail

#ifdef __wasi__
export namespace phenotype::detail {
inline void wasi_emit_clear(Color c) { emit_clear(g_wasi, c); }
inline void wasi_flush_if_changed() { flush_if_changed(g_wasi); }
inline void wasi_paint_node(NodeHandle h, float ox, float oy, float sy, float vh) {
    paint_node(g_wasi, g_wasi, h, ox, oy, sy, vh);
}
}
#endif

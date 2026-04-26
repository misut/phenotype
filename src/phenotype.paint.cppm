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
        // A mid-paint flush invalidates every paint_offset recorded so
        // far this frame — those offsets referred to bytes that have
        // been consumed and cleared from r.buf(). paint_node compares
        // this epoch at entry and exit to know whether its recorded
        // range is still trustworthy.
        ++g_app.paint_flush_epoch;
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

// Clips all subsequent draw commands to the given rect until the next
// Scissor command. Backends (Metal, D3D12, Vulkan, WebGPU) do not
// support nested scissor regions — emit a reset before the next
// scope rather than a stack-push.
template <render_backend R>
void emit_scissor(R& r, float x, float y, float w, float h) {
    detail::ensure(r, 20);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::Scissor));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
}

// Zero-sized Scissor payload — backends read (w == 0 && h == 0) as
// "restore full-viewport clipping." Paired with emit_scissor around a
// dirty-root subtree.
template <render_backend R>
void emit_scissor_reset(R& r) {
    emit_scissor(r, 0.0f, 0.0f, 0.0f, 0.0f);
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
    auto& app = detail::g_app;

    // Snapshot the current frame's byte stream before flush() resets
    // buf_len. The subtree paint cache (see paint_node entry guard)
    // blits ranges out of this snapshot on the next frame, so every
    // frame — including flush-skipped ones — must keep prev_cmd_buf
    // mirrored with the most recent "final" buffer contents.
    auto len = r.buf_len();
    if (len > AppState::PAINT_CACHE_BUF_SIZE) len = AppState::PAINT_CACHE_BUF_SIZE;
    std::memcpy(app.prev_cmd_buf, r.buf(), len);
    app.prev_cmd_len = len;

    auto hash = detail::fnv1a_64(r.buf(), r.buf_len());
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
void emit_focused_input_selection(R& r,
                                  M const& measurer,
                                  FocusedInputSnapshot const& snapshot,
                                  float scroll_x,
                                  float scroll_y) {
    if (!snapshot.valid || !snapshot.selection_active || snapshot.value.empty())
        return;

    auto start = static_cast<std::size_t>(snapshot.selection_start);
    auto end = static_cast<std::size_t>(snapshot.selection_end);
    if (end <= start)
        return;

    float base_x = snapshot.x + snapshot.padding[3] - scroll_x;
    float draw_y = snapshot.y - scroll_y + snapshot.padding[0];
    float start_x = base_x + measurer.measure_text(
        snapshot.font_size,
        snapshot.mono ? 1u : 0u,
        snapshot.value.data(),
        static_cast<unsigned int>(start));
    float end_x = base_x + measurer.measure_text(
        snapshot.font_size,
        snapshot.mono ? 1u : 0u,
        snapshot.value.data(),
        static_cast<unsigned int>(end));
    float width = end_x - start_x;
    if (width <= 0.0f)
        return;

    auto color = snapshot.accent;
    color.a = 72;
    emit_fill_rect(r, start_x, draw_y, width, snapshot.line_height, color);
}

// Collects callback_ids for focusable nodes in document order. Invoked
// once per frame from the runner before paint so focus/Tab order is
// preserved even when paint_node blits a subtree's cached bytes
// (blit skips the tree walk and would otherwise skip the push_back).
inline void collect_focusable_ids(NodeHandle node_h) {
    auto& node = node_at(node_h);
    if (node.callback_id != 0xFFFFFFFF && node.focusable)
        g_app.focusable_ids.push_back(node.callback_id);
    for (auto child_h : node.children)
        collect_focusable_ids(child_h);
}

inline std::uint64_t callback_mask_bit(unsigned int callback_id) noexcept {
    if (callback_id == 0xFFFFFFFFu) return 0;
    return 1ULL << (callback_id & 63u);
}

template <render_backend R, text_measurer M>
void paint_node(R& r, M const& measurer, NodeHandle node_h,
                float ox, float oy,
                float scroll_x, float scroll_y,
                float vp_width, float vp_height) {
    auto& node = node_at(node_h);
    float ax = ox + node.x;
    float ay = oy + node.y;

    if (node.children.empty()) {
        if (ay + node.height < scroll_y || ay > scroll_y + vp_height) return;
        if (ax + node.width  < scroll_x || ax > scroll_x + vp_width)  return;
    }

    // ---- Subtree paint cache blit guard ----------------------------
    // If diff copied this subtree's layout AND every paint-ambient
    // input is unchanged since the frame that emitted it, memcpy the
    // saved byte range out of g_app.prev_cmd_buf instead of re-walking.
    // Byte-exact reuse: the bytes were emitted with identical ax/ay/
    // scroll_x/scroll_y and no intersecting hover/focus transition, so
    // they are byte-for-byte what this walk would produce.
    if (node.layout_valid && node.paint_valid
        && ax == node.paint_ax
        && ay == node.paint_ay
        && scroll_x == g_app.prev_scroll_x
        && scroll_y == g_app.prev_scroll_y
        && (node.paint_callback_mask & g_app.paint_invalidation_mask) == 0
        && node.paint_offset + node.paint_length <= g_app.prev_cmd_len
        && node.paint_length > 0)
    {
        auto const entry_flush_epoch = g_app.paint_flush_epoch;
        auto const len = node.paint_length;
        auto const write_pos = r.buf_len();
        ensure(r, len);
        if (g_app.paint_flush_epoch == entry_flush_epoch
            && r.buf_len() + len <= r.buf_size())
        {
            std::memcpy(&r.buf()[write_pos],
                        &g_app.prev_cmd_buf[node.paint_offset], len);
            r.buf_len() += len;
            // Update paint_offset to this frame's *write* position so
            // next frame's prev_cmd_buf — which captures the current
            // buf verbatim — has this subtree's bytes at the offset we
            // just stored. Without this, a sibling of a blitted node
            // shifting (e.g. adding a focus border) would leave stale
            // offsets pointing into unrelated bytes from the old layout.
            node.paint_offset = write_pos;
            metrics::inst::paint_subtrees_blitted.add();
            metrics::inst::paint_bytes_blitted.add(len);
            return;
        }
    }

    // ---- Miss path: walk, emit, and record paint cache state -------
    auto const before = r.buf_len();
    auto const entry_flush_epoch = g_app.paint_flush_epoch;
    std::uint64_t subtree_mask = callback_mask_bit(node.callback_id);

    float draw_x = ax - scroll_x;
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
            emit_round_rect(r, draw_x, draw_y, node.width, node.height,
                            node.border_radius, bg);
        else
            emit_fill_rect(r, draw_x, draw_y, node.width, node.height, bg);
    }

    if (node.decoration != Decoration::None) {
        Color decoration_color = g_app.theme.surface;
        if (node.decoration == Decoration::Check) {
            float cx = draw_x + node.width * 0.5f;
            float cy = draw_y + node.height * 0.5f;
            float u  = node.width;
            emit_draw_line(r, cx - u * 0.25f, cy + u * 0.02f,
                           cx - u * 0.05f, cy + u * 0.18f, 2.0f, decoration_color);
            emit_draw_line(r, cx - u * 0.05f, cy + u * 0.18f,
                           cx + u * 0.28f, cy - u * 0.18f, 2.0f, decoration_color);
        } else {
            float dot = node.width * 0.4f;
            float dx  = draw_x + (node.width  - dot) * 0.5f;
            float dy  = draw_y + (node.height - dot) * 0.5f;
            emit_round_rect(r, dx, dy, dot, dot, dot * 0.5f, decoration_color);
        }
    }

    Color bc = is_focused ? g_app.theme.state_focus_ring : node.border_color;
    float bw = is_focused ? g_app.theme.state_focus_ring_width : node.border_width;
    if (bw > 0 && bc.a > 0) {
        emit_stroke_rect(r, draw_x, draw_y, node.width, node.height, bw, bc);
    }

    bool html_overlay_active = false;
#ifdef __wasi__
    html_overlay_active = is_focused && node.is_input;
#endif
    Color tc = (is_hovered && node.hover_text_color.a > 0)
        ? node.hover_text_color : node.text_color;
    if (!html_overlay_active && is_focused && node.is_input) {
        auto snapshot = focused_input_snapshot();
        if (snapshot.valid && snapshot.callback_id == node.callback_id)
            emit_focused_input_selection(r, measurer, snapshot, scroll_x, scroll_y);
    }
    if (!html_overlay_active && !node.text_lines.empty()) {
        float line_height = node.font_size * g_app.theme.line_height_ratio;
        float inner_width = node.width - node.style.padding[1] - node.style.padding[3];
        float ty = draw_y + node.style.padding[0];
        for (auto const& line : node.text_lines) {
            if (!line.empty()) {
                float line_w = measurer.measure_text(
                    node.font_size, node.mono ? 1 : 0,
                    line.c_str(), static_cast<unsigned int>(line.size()));
                float tx = draw_x + node.style.padding[3];
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
        emit_draw_image(r, draw_x, draw_y, node.width, node.height,
                        node.image_url.c_str(),
                        static_cast<unsigned int>(node.image_url.size()));
    }

    // Immediate-mode paint hook (widget::canvas). The Painter sees a
    // local coordinate system whose origin sits at this node's
    // resolved (x, y); it forwards each call to emit_draw_line in
    // absolute screen coords (already accounting for scroll).
    if (node.paint_fn) {
        struct PainterImpl : public Painter {
            R& r;
            float origin_x;
            float origin_y;
            PainterImpl(R& r_in, float ox, float oy)
                : r(r_in), origin_x(ox), origin_y(oy) {}
            void line(float x1, float y1, float x2, float y2,
                      float thickness, Color color) override {
                emit_draw_line(r,
                               origin_x + x1, origin_y + y1,
                               origin_x + x2, origin_y + y2,
                               thickness, color);
            }
            void text(float x, float y,
                      char const* str, unsigned int len,
                      float font_size, Color color) override {
                emit_draw_text(r,
                               origin_x + x, origin_y + y,
                               font_size, /*mono=*/0u,
                               color, str, len);
            }
        };
        PainterImpl painter(r, draw_x, draw_y);
        node.paint_fn(painter);
    }

    if (node.callback_id != 0xFFFFFFFF) {
        emit_hit_region(r, ax, ay, node.width, node.height,
                        node.callback_id, node.cursor_type);
    }

    // Dirty-root detection for scissor. A child is a dirty root when
    // its own layout_valid is false but at least one sibling will blit
    // (sibling.layout_valid == true) — i.e. the child is the sole
    // delta inside an otherwise-clean parent.children set. Wrap each
    // such child in a scissor so the backend's rasteriser only has to
    // touch pixels that actually changed this frame. The four target
    // graphics APIs do not support nested scissor regions, so we only
    // wrap when not already inside one (tracked via the paint-cache's
    // scissor_depth on g_app).
    bool any_sibling_blits = false;
    for (auto child_h : node.children) {
        if (node_at(child_h).layout_valid) { any_sibling_blits = true; break; }
    }

    for (auto child_h : node.children) {
        auto const& child = node_at(child_h);
        bool const child_is_dirty_root =
            !child.layout_valid
            && any_sibling_blits
            && g_app.paint_scissor_depth == 0;

        if (child_is_dirty_root) {
            float cx = ax + child.x - scroll_x;
            float cy = ay + child.y - scroll_y;
            emit_scissor(r, cx, cy, child.width, child.height);
            ++g_app.paint_scissor_depth;
            metrics::inst::scissor_emitted.add();
        }

        paint_node(r, measurer, child_h, ax, ay,
                   scroll_x, scroll_y, vp_width, vp_height);
        subtree_mask |= node_at(child_h).paint_callback_mask;

        if (child_is_dirty_root) {
            emit_scissor_reset(r);
            --g_app.paint_scissor_depth;
        }
    }

    // Record paint-cache state for the next frame's diff to copy and
    // the next frame's paint_node entry guard to consult. If ensure()
    // had to flush mid-walk (buf_len reset to 0), the recorded offset
    // would be meaningless; skip caching in that case. Comparing
    // paint_flush_epoch against entry_flush_epoch catches the case
    // where a mid-walk flush was followed by enough re-emit to make
    // after >= before numerically, which the `after >= before` check
    // alone would silently accept as valid.
    auto const after = r.buf_len();
    if (after >= before && g_app.paint_flush_epoch == entry_flush_epoch) {
        node.paint_offset = before;
        node.paint_length = after - before;
        node.paint_ax = ax;
        node.paint_ay = ay;
        node.paint_callback_mask = subtree_mask;
        node.paint_valid = true;
    } else {
        node.paint_valid = false;
    }
}

} // namespace phenotype::detail

#ifdef __wasi__
export namespace phenotype::detail {
inline void wasi_emit_clear(Color c) { emit_clear(g_wasi, c); }
inline void wasi_flush_if_changed() { flush_if_changed(g_wasi); }
inline void wasi_paint_node(NodeHandle h, float ox, float oy,
                            float sx, float sy, float vw, float vh) {
    paint_node(g_wasi, g_wasi, h, ox, oy, sx, sy, vw, vh);
}
}
#endif

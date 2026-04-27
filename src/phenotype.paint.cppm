module;
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
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

// Stroked arc — `start_angle` to `end_angle` in radians (CCW). Backends
// rasterise as an SDF in the fragment shader, so zoom-in stays smooth
// without parse-time chord tessellation. Layout: 32 bytes (opcode +
// 6×f32 + packed RGBA).
template <render_backend R>
void emit_draw_arc(R& r, float cx, float cy, float radius,
                   float start_angle, float end_angle,
                   float thickness, Color c) {
    detail::ensure(r, 32);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::DrawArc));
    detail::write_f32(r, cx); detail::write_f32(r, cy);
    detail::write_f32(r, radius);
    detail::write_f32(r, start_angle); detail::write_f32(r, end_angle);
    detail::write_f32(r, thickness);
    detail::write_u32(r, c.packed());
}

// ============================================================
// Path — variable-length verb stream
// ============================================================
//
// `PathBuilder` lives in `phenotype.types` (alongside `Painter`).
// The emit functions below memcpy its packed verb words straight
// into the command buffer. See `Cmd::Path` / `Cmd::FillPath` for the
// wire layout.

// Stroked path. Backends CPU-flatten curve segments into the existing
// line / arc instance buffers (no new pipeline). Layout: opcode +
// thickness (f32) + packed RGBA + verb_count (u32) + verb_count
// inline verbs (each `verbs[i]` is one u32 word, see `PathBuilder`).
template <render_backend R>
void emit_stroke_path(R& r, PathBuilder const& path,
                      float thickness, Color c) {
    auto const words = static_cast<unsigned int>(path.verbs.size());
    detail::ensure(r, 16 + words * 4);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::Path));
    detail::write_f32(r, thickness);
    detail::write_u32(r, c.packed());
    detail::write_u32(r, path.verb_count);
    for (auto w : path.verbs) detail::write_u32(r, w);
}

// Filled path. Single closed loop only — self-intersection / multi-
// loop / hole semantics are out of scope for this slab and land with
// HATCH support later. Layout: opcode + packed RGBA + verb_count +
// inline verbs (no thickness slot).
template <render_backend R>
void emit_fill_path(R& r, PathBuilder const& path, Color c) {
    auto const words = static_cast<unsigned int>(path.verbs.size());
    detail::ensure(r, 12 + words * 4);
    detail::write_u32(r, static_cast<unsigned int>(Cmd::FillPath));
    detail::write_u32(r, c.packed());
    detail::write_u32(r, path.verb_count);
    for (auto w : path.verbs) detail::write_u32(r, w);
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
        && !node.paint_fn
        && !node.paint_dynamic
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
    bool subtree_dynamic = static_cast<bool>(node.paint_fn);

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
        float inner_height = node.height - node.style.padding[0] - node.style.padding[2];
        float text_block_height = line_height
            * static_cast<float>(node.text_lines.size());
        float vertical_offset = 0.0f;
        if (inner_height > text_block_height)
            vertical_offset = (inner_height - text_block_height) / 2.0f;
        float ty = draw_y + node.style.padding[0] + vertical_offset;
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
        // CPU-side clipping for everything the canvas emits — apps
        // doing pan / zoom inside the canvas (cad++, charts, custom
        // viewports) routinely have geometry that lives outside the
        // visible region, and without clipping those draws bleed onto
        // the rest of the layout tree. The renderer's `Cmd::Scissor`
        // opcode is currently decode-and-skip on both Metal and
        // Vulkan backends (a follow-up will issue actual
        // setScissorRect / vkCmdSetScissor between batches), so we
        // also do the clipping right here at emit time:
        //   * lines  → Cohen–Sutherland against the canvas's
        //              absolute bounds; segments entirely outside are
        //              suppressed, partially-out segments are
        //              shortened to their visible extent before
        //              `emit_draw_line` writes them into the cmd
        //              buffer.
        //   * text   → coarse bbox suppression. Text is rasterised as
        //              a textured quad whose width depends on font
        //              metrics we don't replicate at this layer, so
        //              partial-out runs still emit and render with
        //              their original full extent. Fully-out runs
        //              are dropped, which handles the common
        //              "panned-far-away" case.
        // The companion `emit_scissor` call before the paint_fn keeps
        // the wire format honest and lets a future renderer-side
        // implementation kick in automatically — the CPU pass below
        // will simply become a fast-path filter at that point.
        struct PainterImpl : public Painter {
            R& r;
            float origin_x;
            float origin_y;
            float clip_x0, clip_y0, clip_x1, clip_y1;

            PainterImpl(R& r_in, float ox, float oy,
                        float cx, float cy, float cw, float ch)
                : r(r_in), origin_x(ox), origin_y(oy),
                  clip_x0(cx), clip_y0(cy),
                  clip_x1(cx + cw), clip_y1(cy + ch) {}

            // Cohen-Sutherland outcode bits — `top` bit means y is
            // SMALLER than y-min in our top-down coordinate system,
            // so the names match the user's mental model rather than
            // textbook math conventions. Plain enum (not
            // `static constexpr int`) because PainterImpl is a
            // function-local struct, where C++ disallows static data
            // members.
            enum : int {
                CS_INSIDE = 0,
                CS_LEFT   = 1,
                CS_RIGHT  = 2,
                CS_TOP    = 4,
                CS_BOTTOM = 8,
            };

            int outcode(float x, float y) const {
                int code = CS_INSIDE;
                if (x < clip_x0) code |= CS_LEFT;
                else if (x > clip_x1) code |= CS_RIGHT;
                if (y < clip_y0) code |= CS_TOP;
                else if (y > clip_y1) code |= CS_BOTTOM;
                return code;
            }

            // Cohen–Sutherland clip; trims (x1, y1)–(x2, y2) to lie
            // inside the canvas rect. Returns false when the whole
            // line is on the same outside half-plane (trivial reject)
            // or degenerated to a sub-pixel after clipping.
            bool clip_line(float& x1, float& y1,
                           float& x2, float& y2) const {
                int c1 = outcode(x1, y1);
                int c2 = outcode(x2, y2);
                while (true) {
                    if ((c1 | c2) == 0) return true;
                    if ((c1 & c2) != 0) return false;
                    int co = c1 ? c1 : c2;
                    float x = 0.0f, y = 0.0f;
                    if (co & CS_TOP) {
                        if (y2 == y1) return false;
                        x = x1 + (x2 - x1) * (clip_y0 - y1) / (y2 - y1);
                        y = clip_y0;
                    } else if (co & CS_BOTTOM) {
                        if (y2 == y1) return false;
                        x = x1 + (x2 - x1) * (clip_y1 - y1) / (y2 - y1);
                        y = clip_y1;
                    } else if (co & CS_RIGHT) {
                        if (x2 == x1) return false;
                        y = y1 + (y2 - y1) * (clip_x1 - x1) / (x2 - x1);
                        x = clip_x1;
                    } else { // CS_LEFT
                        if (x2 == x1) return false;
                        y = y1 + (y2 - y1) * (clip_x0 - x1) / (x2 - x1);
                        x = clip_x0;
                    }
                    if (co == c1) {
                        x1 = x; y1 = y; c1 = outcode(x1, y1);
                    } else {
                        x2 = x; y2 = y; c2 = outcode(x2, y2);
                    }
                }
            }

            void line(float x1, float y1, float x2, float y2,
                      float thickness, Color color) override {
                float ax1 = origin_x + x1, ay1 = origin_y + y1;
                float ax2 = origin_x + x2, ay2 = origin_y + y2;
                if (!clip_line(ax1, ay1, ax2, ay2)) return;
                emit_draw_line(r, ax1, ay1, ax2, ay2, thickness, color);
            }

            void arc(float cx, float cy, float radius,
                     float start_angle, float end_angle,
                     float thickness, Color color) override {
                // Drop arcs whose stroke bbox lies entirely outside the
                // canvas — the backend's SDF fragment shader clips
                // partials at the scissor rect pixel-perfectly.
                // Half-thickness padding handles the outer edge of the
                // stroke spilling past `radius`.
                if (radius <= 0.0f) return;
                float acx = origin_x + cx;
                float acy = origin_y + cy;
                float pad = radius + thickness * 0.5f;
                if (acx + pad <= clip_x0
                    || acx - pad >= clip_x1
                    || acy + pad <= clip_y0
                    || acy - pad >= clip_y1) {
                    return;
                }
                emit_draw_arc(r, acx, acy, radius,
                              start_angle, end_angle, thickness, color);
            }

            void stroke_path(PathBuilder const& path,
                             float thickness, Color color) override {
                if (path.empty()) return;
                // PathBuilder coordinates are canvas-local; translate
                // them to surface-local with the same `origin_x /
                // origin_y` the line / arc paths apply. Bbox cull is
                // out of scope for now — the surrounding `Cmd::Scissor`
                // (emitted around every canvas's paint_fn) clips
                // anything outside the canvas rect pixel-perfectly,
                // and CAD paths are dominated by visible-on-screen
                // entities so a cheap walk-once cull will only pay for
                // itself once paint becomes a profile hotspot.
                auto translated = path.translated(origin_x, origin_y);
                emit_stroke_path(r, translated, thickness, color);
            }

            void text(float x, float y,
                      char const* str, unsigned int len,
                      float font_size, Color color) override {
                // Coarse bbox: assume each character is ~0.6×font_size
                // wide. Drop runs whose approximated advance box lies
                // entirely outside the canvas; partials still emit and
                // the backend's `setScissorRect` / `vkCmdSetScissor`
                // call (Cmd::Scissor, applied since phenotype#202)
                // clips them pixel-perfectly at draw time.
                //
                // (Earlier this gate had to use strict containment as
                // a stop-gap, because the Cmd::Scissor opcode was
                // decode-and-skip on every backend — so partial runs
                // would rasterise glyphs across the canvas boundary.
                // With real backend scissor in place that workaround
                // is no longer needed, and the strict check would
                // visibly drop legitimate text the instant any edge
                // crossed the boundary.)
                float ax = origin_x + x;
                float ay = origin_y + y;
                float approx_w = font_size *
                                 static_cast<float>(len) * 0.6f;
                float approx_h = font_size;
                if (ax           >= clip_x1
                    || ax + approx_w <= clip_x0
                    || ay            >= clip_y1
                    || ay + approx_h <= clip_y0) {
                    return;
                }
                emit_draw_text(r,
                               origin_x + x, origin_y + y,
                               font_size, /*mono=*/0u,
                               color, str, len);
            }
        };

        bool emit_canvas_scissor = (g_app.paint_scissor_depth == 0);
        if (emit_canvas_scissor) {
            emit_scissor(r, ax, ay, node.width, node.height);
            ++g_app.paint_scissor_depth;
            metrics::inst::scissor_emitted.add();
        }
        PainterImpl painter(r, draw_x, draw_y, ax, ay,
                            node.width, node.height);
        node.paint_fn(painter);
        if (emit_canvas_scissor) {
            emit_scissor_reset(r);
            --g_app.paint_scissor_depth;
        }
    }

    if (node.callback_id != 0xFFFFFFFF) {
        emit_hit_region(r, ax, ay, node.width, node.height,
                        node.callback_id, node.cursor_type);
    }

    // Register this canvas as the active gesture target — the shell
    // looks at `g_app.gesture_target_*` to route platform pinch / pan
    // events. Last canvas painted wins on the rare apps that have
    // multiple gesture-aware canvases on screen at once; that policy
    // matches z-order intuition for nested canvases (the inner one
    // paints last). `ax` / `ay` are the canvas's top-left in
    // post-scroll surface coords, matching what `dispatch_gesture`
    // receives from the input backend.
    if (node.gesture_callback_id != 0xFFFFFFFFu) {
        g_app.gesture_target_id = node.gesture_callback_id;
        g_app.gesture_target_x  = ax;
        g_app.gesture_target_y  = ay;
        g_app.gesture_target_w  = node.width;
        g_app.gesture_target_h  = node.height;
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
        subtree_dynamic = subtree_dynamic || node_at(child_h).paint_dynamic;

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
    node.paint_dynamic = subtree_dynamic;
    if (!subtree_dynamic && after >= before
        && g_app.paint_flush_epoch == entry_flush_epoch) {
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

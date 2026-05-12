module;
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <string_view>
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

struct FillRectWire {
    float x;
    float y;
    float w;
    float h;
    unsigned int color;
};
static_assert(sizeof(FillRectWire) == 20);

// ---- Buffer write helpers (templated on render_backend) ----

inline char const* opcode_name(Cmd c) noexcept {
    switch (c) {
        case Cmd::Clear:      return "Clear";
        case Cmd::FillRect:   return "FillRect";
        case Cmd::StrokeRect: return "StrokeRect";
        case Cmd::RoundRect:  return "RoundRect";
        case Cmd::DrawText:   return "DrawText";
        case Cmd::DrawLine:   return "DrawLine";
        case Cmd::HitRegion:  return "HitRegion";
        case Cmd::DrawImage:  return "DrawImage";
        case Cmd::Scissor:    return "Scissor";
        case Cmd::DrawArc:    return "DrawArc";
        case Cmd::Path:       return "Path";
        case Cmd::FillPath:   return "FillPath";
        case Cmd::FillQuads:  return "FillQuads";
        case Cmd::FillRects:  return "FillRects";
    }
    return "Unknown";
}

// Surface a true buffer overflow (single command exceeds the entire
// fixed buffer even after a mid-paint flush). Currently the buffer is
// `unsigned char buffer[BUF_SIZE]` with BUF_SIZE = 65536 across every
// host (null_host, native_host, wasi_paint_host). Without this guard,
// the subsequent write_u32 / write_bytes inside the calling emit_*
// would walk past the end of the fixed array — silent memory
// corruption. Caller drops the command instead.
//
// Bumping `paint_flush_epoch` is load-bearing: the cache-recording
// block at the bottom of paint_node compares the entry epoch against
// the current one to decide whether to mark `paint_valid = true`. If
// we did not bump on overflow, a partial byte range (everything
// emitted up to the dropped command) would be cached and reused next
// frame, masking the regression visually.
inline void report_paint_overflow(Cmd opcode, unsigned int needed,
                                   unsigned int buf_len,
                                   unsigned int buf_size) noexcept {
    auto const op_name = opcode_name(opcode);
    metrics::inst::paint_buffer_overflow.add(
        1, {{"opcode", op_name}});
    log::error("phenotype.paint",
        "command stream overflow: opcode={} ({}) needed={} buf_len={} "
        "buf_size={} canvas={{cb={}, ax={}, ay={}, w={}, h={}}}",
        static_cast<unsigned int>(opcode), op_name,
        needed, buf_len, buf_size,
        g_app.current_paint_callback_id,
        g_app.current_paint_ax, g_app.current_paint_ay,
        g_app.current_paint_w,  g_app.current_paint_h);
    ++g_app.paint_flush_epoch;
#ifndef NDEBUG
    if (g_app.diag_abort_on_paint_overflow) {
        // Flush every open stream so the [ERROR] line above survives
        // the abort under shell-redirected stderr (where libc may
        // block-buffer instead of line-buffering). Without this, the
        // diagnostic line that tells the user what crashed disappears.
        std::fflush(nullptr);
        std::abort();
    }
#endif
}

template <render_backend R>
[[nodiscard]] bool ensure(R& r, unsigned int needed, Cmd opcode) noexcept {
    if (r.buf_len() + needed > r.buf_size()) {
        // Prefer grow over mid-paint flush. Empirically, a single
        // backend frame that ends up split across multiple flushes
        // loses every command emitted before the last flush — for
        // example, cad++ loading colorwh.dwg renders the wheel
        // (last hatch batch) but every preceding widget (heading,
        // summary card, sidebar) disappears, even though the host
        // returned success on every flush call. Keeping the whole
        // frame in one contiguous buffer until end-of-paint sidesteps
        // that whole class of backend-state corruption.
        if (r.reserve(r.buf_len() + needed)) {
            // Successful grow may have moved r.buf() — every emit_*
            // in this file calls r.buf() per-write, so cached
            // pointers in the caller are not a concern.
            return true;
        }
        // Grow refused (cap exceeded). Fall through to the legacy
        // mid-paint flush path; some backends will recover, the
        // overflow case at least gets logged loudly.
        r.flush();
        r.buf_len() = 0;
        // A mid-paint flush invalidates every paint_offset recorded
        // so far this frame — those offsets referred to bytes that
        // have been consumed and cleared from r.buf(). paint_node
        // compares this epoch at entry and exit to know whether its
        // recorded range is still trustworthy.
        ++g_app.paint_flush_epoch;
        metrics::inst::paint_buffer_flushes.add();
    }
    if (r.buf_len() + needed > r.buf_size()) {
        report_paint_overflow(opcode, needed, r.buf_len(), r.buf_size());
        return false;
    }
    return true;
}

// Cache-blit overload — the bytes being written are a memcpy of a
// previously-recorded subtree, not a single typed command, so there
// is no opcode to report. Returns whether the blit fits; the caller
// at paint_node already has a fall-through to the miss-path walk
// when this returns false (or when paint_flush_epoch advances mid-
// blit), so we deliberately do NOT emit an overflow log here.
template <render_backend R>
[[nodiscard]] bool ensure_blit(R& r, unsigned int needed) noexcept {
    if (r.buf_len() + needed > r.buf_size()) {
        if (r.reserve(r.buf_len() + needed)) return true;
        r.flush();
        r.buf_len() = 0;
        ++g_app.paint_flush_epoch;
        metrics::inst::paint_buffer_flushes.add();
    }
    return r.buf_len() + needed <= r.buf_size();
}

// RAII guard that pushes the currently-painting node's identity into
// `g_app.current_paint_*` on entry and restores the previous value on
// exit. paint_node is recursive, so the snapshot/restore lets the log
// line in report_paint_overflow always name the *deepest* node being
// processed when the overflow happens — which is the canvas / widget
// the user actually has on screen.
struct PaintCtxGuard {
    unsigned int prev_cb;
    float        prev_ax, prev_ay, prev_w, prev_h;

    PaintCtxGuard(unsigned int cb, float ax, float ay,
                   float w, float h) noexcept
        : prev_cb{g_app.current_paint_callback_id},
          prev_ax{g_app.current_paint_ax},
          prev_ay{g_app.current_paint_ay},
          prev_w {g_app.current_paint_w},
          prev_h {g_app.current_paint_h}
    {
        g_app.current_paint_callback_id = cb;
        g_app.current_paint_ax = ax;
        g_app.current_paint_ay = ay;
        g_app.current_paint_w  = w;
        g_app.current_paint_h  = h;
    }

    ~PaintCtxGuard() {
        g_app.current_paint_callback_id = prev_cb;
        g_app.current_paint_ax = prev_ax;
        g_app.current_paint_ay = prev_ay;
        g_app.current_paint_w  = prev_w;
        g_app.current_paint_h  = prev_h;
    }

    PaintCtxGuard(PaintCtxGuard const&) = delete;
    PaintCtxGuard& operator=(PaintCtxGuard const&) = delete;
};

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
    float measure_text(float fs, FontSpec font,
                       char const* t, unsigned int l) const {
        unsigned int flags =
            (font.mono ? 1u : 0u)
            | (font.weight == FontWeight::Bold  ? 2u : 0u)
            | (font.style  == FontStyle::Italic ? 4u : 0u);
        return phenotype_measure_text(
            fs, flags,
            font.family.data(),
            static_cast<unsigned int>(font.family.size()),
            t, l);
    }

    // No wasm-side font-metric host yet; the WASI text renderer is
    // canvas-2D-backed, where the layout consumer (cadpp on wasm) has
    // its own fallback. Returning zero matches the existing
    // "no measurement capability" contract from `measure_text`.
    FontMetrics font_metrics(float /*fs*/, FontSpec /*font*/) const {
        return {};
    }
    unsigned char* buf() { return phenotype_cmd_buf; }
    unsigned int& buf_len() { return phenotype_cmd_len; }
    unsigned int buf_size() { return BUF_SIZE; }
    void flush() {
        if (phenotype_cmd_len > 0) {
            phenotype_flush(); phenotype_cmd_len = 0;
        }
    }
    // wasm host can't grow — `phenotype_cmd_buf` is a known global
    // address shared with the JS reader, and the JS side reads
    // BUF_SIZE bytes per flush. Future protocol change (e.g. a
    // growable shared-memory region or a per-flush size export)
    // would unlock this; until then, a single emit_* whose payload
    // exceeds BUF_SIZE on web hits the existing overflow-report
    // path instead of growing.
    [[nodiscard]] bool reserve(unsigned int needed) {
        return needed <= BUF_SIZE;
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
    if (!detail::ensure(r, 8, Cmd::Clear)) return;
    detail::write_u32(r, static_cast<unsigned int>(Cmd::Clear));
    detail::write_u32(r, c.packed());
}

template <render_backend R>
void emit_fill_rect(R& r, float x, float y, float w, float h, Color c) {
    if (!detail::ensure(r, 24, Cmd::FillRect)) return;
    detail::write_u32(r, static_cast<unsigned int>(Cmd::FillRect));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_u32(r, c.packed());
}

template <render_backend R>
void emit_stroke_rect(R& r, float x, float y, float w, float h, float lw, Color c) {
    if (!detail::ensure(r, 28, Cmd::StrokeRect)) return;
    detail::write_u32(r, static_cast<unsigned int>(Cmd::StrokeRect));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_f32(r, lw);
    detail::write_u32(r, c.packed());
}

template <render_backend R>
void emit_round_rect(R& r, float x, float y, float w, float h, float radius, Color c) {
    if (!detail::ensure(r, 28, Cmd::RoundRect)) return;
    detail::write_u32(r, static_cast<unsigned int>(Cmd::RoundRect));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_f32(r, radius);
    detail::write_u32(r, c.packed());
}

// Pack a FontSpec's mono / weight / italic bits into the wire-format
// `flags` u32. Bit 0 = mono, bit 1 = bold, bit 2 = italic, bits 3..31
// reserved (must be zero on emit, ignored on decode). Layout matches
// the `parseCommands` decoder in shim/phenotype.js so wasm + native
// agree byte-for-byte.
constexpr unsigned int pack_font_flags(FontSpec const& f) noexcept {
    return (f.mono ? 1u : 0u)
        | (f.weight == FontWeight::Bold  ? 2u : 0u)
        | (f.style  == FontStyle::Italic ? 4u : 0u);
}

// Emit a `Cmd::DrawText` payload. Wire layout (all little-endian):
//   u32  opcode = 5
//   f32  x
//   f32  y
//   f32  font_size
//   f32  rotation       // radians, CCW about pivot `(x, y)`; 0 = upright
//   f32  width_factor   // horizontal glyph stretch; 1 = native advance
//   u32  flags          // bit0=mono, bit1=bold, bit2=italic
//   u32  color (RGBA packed)
//   u32  family_len     // 0 = backend default family
//   u8[] family bytes (padded up to 4)
//   u32  text_len
//   u8[] text bytes (padded up to 4)
//
// Fixed overhead = 40 bytes (was 36 — `width_factor` is the new f32
// slot after `rotation`; default-1.0 round-trips the pre-stretch
// rendering for callers that still pass FontSpec without setting it).
// Family + text payloads each pad up to a 4-byte boundary independently
// so the next opcode starts aligned.
template <render_backend R>
void emit_draw_text(R& r, float x, float y, float font_size, float rotation,
                    float width_factor,
                    unsigned int flags,
                    Color c, std::string_view family,
                    char const* text, unsigned int text_len) {
    auto const family_len = static_cast<unsigned int>(family.size());
    if (!detail::ensure(r, 40 + detail::padded(family_len) + detail::padded(text_len),
                        Cmd::DrawText)) return;
    detail::write_u32(r, static_cast<unsigned int>(Cmd::DrawText));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, font_size);
    detail::write_f32(r, rotation);
    detail::write_f32(r, width_factor);
    detail::write_u32(r, flags);
    detail::write_u32(r, c.packed());
    detail::write_u32(r, family_len);
    detail::write_bytes(r, family.data(), family_len);
    detail::write_u32(r, text_len);
    detail::write_bytes(r, text, text_len);
}

template <render_backend R>
void emit_draw_image(R& r, float x, float y, float w, float h,
                     char const* url, unsigned int len) {
    if (!detail::ensure(r, 24 + detail::padded(len), Cmd::DrawImage)) return;
    detail::write_u32(r, static_cast<unsigned int>(Cmd::DrawImage));
    detail::write_f32(r, x); detail::write_f32(r, y);
    detail::write_f32(r, w); detail::write_f32(r, h);
    detail::write_u32(r, len);
    detail::write_bytes(r, url, len);
}

template <render_backend R>
void emit_draw_line(R& r, float x1, float y1, float x2, float y2,
                    float thickness, Color c) {
    if (!detail::ensure(r, 28, Cmd::DrawLine)) return;
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
    if (!detail::ensure(r, 32, Cmd::DrawArc)) return;
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
    if (!detail::ensure(r, 16 + words * 4, Cmd::Path)) return;
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
    if (!detail::ensure(r, 12 + words * 4, Cmd::FillPath)) return;
    detail::write_u32(r, static_cast<unsigned int>(Cmd::FillPath));
    detail::write_u32(r, c.packed());
    detail::write_u32(r, path.verb_count);
    for (auto w : path.verbs) detail::write_u32(r, w);
}

template <render_backend R>
void emit_fill_quads(R& r, PaintQuad const* quads, unsigned int count) {
    if (!quads || count == 0) return;

    constexpr unsigned int max_quads_per_command = 1024;
    unsigned int offset = 0;
    while (offset < count) {
        unsigned int n = count - offset;
        if (n > max_quads_per_command) n = max_quads_per_command;
        if (!detail::ensure(r, 8 + n * 36, Cmd::FillQuads)) return;
        detail::write_u32(r, static_cast<unsigned int>(Cmd::FillQuads));
        detail::write_u32(r, n);
        for (unsigned int i = 0; i < n; ++i) {
            auto const& q = quads[offset + i];
            detail::write_u32(r, q.color.packed());
            detail::write_f32(r, q.x0); detail::write_f32(r, q.y0);
            detail::write_f32(r, q.x1); detail::write_f32(r, q.y1);
            detail::write_f32(r, q.x2); detail::write_f32(r, q.y2);
            detail::write_f32(r, q.x3); detail::write_f32(r, q.y3);
        }
        offset += n;
    }
}

template <render_backend R>
void emit_fill_quads_translated(R& r, PaintQuad const* quads,
                                unsigned int count,
                                float origin_x, float origin_y) {
    if (!quads || count == 0) return;

    constexpr unsigned int max_quads_per_command = 1024;
    unsigned int offset = 0;
    while (offset < count) {
        unsigned int n = count - offset;
        if (n > max_quads_per_command) n = max_quads_per_command;
        if (!detail::ensure(r, 8 + n * 36, Cmd::FillQuads)) return;
        detail::write_u32(r, static_cast<unsigned int>(Cmd::FillQuads));
        detail::write_u32(r, n);
        for (unsigned int i = 0; i < n; ++i) {
            auto const& q = quads[offset + i];
            detail::write_u32(r, q.color.packed());
            detail::write_f32(r, origin_x + q.x0);
            detail::write_f32(r, origin_y + q.y0);
            detail::write_f32(r, origin_x + q.x1);
            detail::write_f32(r, origin_y + q.y1);
            detail::write_f32(r, origin_x + q.x2);
            detail::write_f32(r, origin_y + q.y2);
            detail::write_f32(r, origin_x + q.x3);
            detail::write_f32(r, origin_y + q.y3);
        }
        offset += n;
    }
}

template <render_backend R>
void emit_fill_rects(R& r, PaintRect const* rects, unsigned int count) {
    if (!rects || count == 0) return;

    constexpr unsigned int max_rects_per_command = 1536;
    unsigned int offset = 0;
    while (offset < count) {
        unsigned int n = count - offset;
        if (n > max_rects_per_command) n = max_rects_per_command;
        if (!detail::ensure(r, 8 + n * 20, Cmd::FillRects)) return;
        detail::write_u32(r, static_cast<unsigned int>(Cmd::FillRects));
        detail::write_u32(r, n);
        for (unsigned int i = 0; i < n; ++i) {
            auto const& rect = rects[offset + i];
            detail::FillRectWire wire{
                rect.x, rect.y, rect.w, rect.h, rect.color.packed()};
            std::memcpy(&r.buf()[r.buf_len()], &wire, sizeof(wire));
            r.buf_len() += static_cast<unsigned int>(sizeof(wire));
        }
        offset += n;
    }
}

template <render_backend R>
void emit_fill_rects_translated(R& r, PaintRect const* rects,
                                unsigned int count,
                                float origin_x, float origin_y) {
    if (!rects || count == 0) return;

    constexpr unsigned int max_rects_per_command = 1536;
    unsigned int offset = 0;
    while (offset < count) {
        unsigned int n = count - offset;
        if (n > max_rects_per_command) n = max_rects_per_command;
        if (!detail::ensure(r, 8 + n * 20, Cmd::FillRects)) return;
        detail::write_u32(r, static_cast<unsigned int>(Cmd::FillRects));
        detail::write_u32(r, n);
        for (unsigned int i = 0; i < n; ++i) {
            auto const& rect = rects[offset + i];
            float x = origin_x + rect.x;
            float y = origin_y + rect.y;
            float w = rect.w;
            float h = rect.h;
            if (w < 0.0f) { x += w; w = -w; }
            if (h < 0.0f) { y += h; h = -h; }
            detail::FillRectWire wire{x, y, w, h, rect.color.packed()};
            std::memcpy(&r.buf()[r.buf_len()], &wire, sizeof(wire));
            r.buf_len() += static_cast<unsigned int>(sizeof(wire));
        }
        offset += n;
    }
}

template <render_backend R>
void emit_hit_region(R& r, float x, float y, float w, float h,
                     unsigned int callback_id, unsigned int cursor_type) {
    if (!detail::ensure(r, 28, Cmd::HitRegion)) return;
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
    if (!detail::ensure(r, 20, Cmd::Scissor)) return;
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

template <render_backend R>
void reset_paint_scissor_boundary(R& r) {
    g_app.paint_scissor_depth = 0;
    emit_scissor_reset(r);
}

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
    FontSpec const input_font{ {}, FontWeight::Regular, FontStyle::Upright,
                               snapshot.mono };
    float start_x = base_x + measurer.measure_text(
        snapshot.font_size, input_font,
        snapshot.value.data(),
        static_cast<unsigned int>(start));
    float end_x = base_x + measurer.measure_text(
        snapshot.font_size, input_font,
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

// After a subtree blit, descendants' paint_offset values still point
// into prev_cmd_buf positions that reflect the *previous* layout — the
// blit replaced their bytes wholesale without rewriting per-descendant
// offsets. Invalidate them so a later frame's blit guard takes the
// miss-path walk for any descendant whose own diff fails while the
// ancestor's blit eligibility lapses. The descendant will re-record
// fresh paint state on that walk and become cache-eligible again.
inline void invalidate_descendant_paint_cache(NodeHandle node_h) {
    auto& node = node_at(node_h);
    for (auto child_h : node.children) {
        auto& child = node_at(child_h);
        child.paint_valid = false;
        invalidate_descendant_paint_cache(child_h);
    }
}

// Subtree blits reuse byte ranges from prev_cmd_buf and skip the normal
// paint walk. Command-buffer side effects like HitRegion are already
// inside the copied bytes, but shell-side routing state (gesture target
// and scroll target lists) lives only in AppState and must be rebuilt
// every frame even when the visual bytes are cached.
inline void register_cached_paint_side_effects(NodeHandle node_h,
                                               float ox, float oy,
                                               float scroll_x,
                                               float scroll_y) {
    auto& node = node_at(node_h);
    float ax = ox + node.x;
    float ay = oy + node.y;

    if (node.gesture_callback_id != 0xFFFFFFFFu) {
        g_app.gesture_target_id = node.gesture_callback_id;
        g_app.gesture_target_x  = ax;
        g_app.gesture_target_y  = ay;
        g_app.gesture_target_w  = node.width;
        g_app.gesture_target_h  = node.height;
    }

    float child_scroll_x = scroll_x;
    float child_scroll_y = scroll_y;
    if (node.is_scroll_container && node.scroll_state) {
        float max_off = std::max(0.0f, node.content_height - node.height);
        float& off = node.scroll_state->offset_y;
        if (off < 0.0f) off = 0.0f;
        if (off > max_off) off = max_off;
        node.scroll_offset_y = off;
        child_scroll_y = scroll_y + off;

        AppState::ScrollTarget tgt;
        tgt.x = ax - scroll_x;
        tgt.y = ay - scroll_y;
        tgt.w = node.width;
        tgt.h = node.height;
        tgt.state = node.scroll_state;
        tgt.content_height = node.content_height;
        g_app.scroll_targets.push_back(tgt);
    }

    for (auto child_h : node.children) {
        register_cached_paint_side_effects(child_h, ax, ay,
                                           child_scroll_x, child_scroll_y);
    }
}

template <render_backend R, text_measurer M>
void paint_node(R& r, M const& measurer, NodeHandle node_h,
                float ox, float oy,
                float scroll_x, float scroll_y,
                float vp_width, float vp_height,
                bool inside_scroll_container = false) {
    auto& node = node_at(node_h);
    float ax = ox + node.x;
    float ay = oy + node.y;

    // Stash this node's identity for report_paint_overflow. RAII so the
    // early-cull return below and any future early returns naturally
    // restore the parent's context on unwind.
    PaintCtxGuard _ctx_guard(node.callback_id, ax, ay, node.width, node.height);

    // A subtree rooted at (or under) a scroll_view is recorded with
    // local-ambient scroll values that don't match `g_app.prev_scroll_y`
    // (which only tracks the root). Skip the cache for such subtrees
    // and never record paint_valid for their nodes — re-walk every
    // frame. The blit cache for nodes outside scroll views still works
    // unchanged.
    bool const effective_inside =
        inside_scroll_container || node.is_scroll_container;

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
    //
    // A widget::canvas paint_fn is normally opaque (forces re-walk),
    // but when the caller opts in via a non-zero paint_token AND the
    // diff carried forward an equal paint_token_prev, the canvas's
    // emitted bytes are declared a pure function of unchanged inputs.
    // We then treat it like a static subtree and blit; paint_fn is
    // not invoked and the byte range (including the canvas-scoped
    // scissor pair) is reused verbatim.
    bool const canvas_token_hit = static_cast<bool>(node.paint_fn)
        && node.paint_token != 0
        && node.paint_token == node.paint_token_prev;
    if (!effective_inside
        && node.layout_valid && node.paint_valid
        && (!node.paint_fn || canvas_token_hit)
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
        // Discard return — the (epoch_changed || not_enough_room) check
        // below already routes around a failed blit by falling through
        // to the miss-path walk. Treating ensure_blit's false return as
        // an early return here would leave the subtree blank.
        (void)ensure_blit(r, len);
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
            invalidate_descendant_paint_cache(node_h);
            register_cached_paint_side_effects(node_h, ox, oy,
                                               scroll_x, scroll_y);
            metrics::inst::paint_subtrees_blitted.add();
            metrics::inst::paint_bytes_blitted.add(len);
            return;
        }
    }

    // ---- Miss path: walk, emit, and record paint cache state -------
    auto const before = r.buf_len();
    auto const entry_flush_epoch = g_app.paint_flush_epoch;
    std::uint64_t subtree_mask = callback_mask_bit(node.callback_id);
    // A canvas paint_fn is dynamic UNLESS the caller opted into the
    // dirty-token contract by passing a non-zero paint_token. The
    // contract says "my emitted bytes are a pure function of this
    // token", so the bytes recorded on this miss-path frame are a
    // legitimate cache for any later frame that produces the same
    // token — even though *this* frame's blit guard already failed
    // (e.g. first frame, token transition, position drift). We mark
    // such subtrees non-dynamic so paint_valid gets recorded below
    // and the next frame's diff/blit can hit.
    bool subtree_dynamic =
        static_cast<bool>(node.paint_fn) && node.paint_token == 0;

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
            float stroke = std::max(u * 0.125f, 1.0f);
            emit_draw_line(r, cx - u * 0.25f, cy + u * 0.02f,
                           cx - u * 0.05f, cy + u * 0.18f, stroke, decoration_color);
            emit_draw_line(r, cx - u * 0.05f, cy + u * 0.18f,
                           cx + u * 0.28f, cy - u * 0.18f, stroke, decoration_color);
        } else {
            float dot = node.width * 0.4f;
            float dx  = draw_x + (node.width  - dot) * 0.5f;
            float dy  = draw_y + (node.height - dot) * 0.5f;
            emit_round_rect(r, dx, dy, dot, dot, dot * 0.5f, decoration_color);
        }
    }

    // Focus chrome (width and colour) is fully view-driven now. Each
    // focusable widget animates its own `border_width` and
    // `border_color` between resting and `theme.state_focus_ring*`
    // via `animate_float` / `animate_color`, so paint just reads the
    // current values.
    if (node.border_width > 0 && node.border_color.a > 0) {
        emit_stroke_rect(r, draw_x, draw_y, node.width, node.height,
                         node.border_width, node.border_color);
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
        FontSpec const node_font{ {}, FontWeight::Regular, FontStyle::Upright,
                                  node.mono };
        unsigned int const node_flags = pack_font_flags(node_font);
        for (auto const& line : node.text_lines) {
            if (!line.empty()) {
                float line_w = measurer.measure_text(
                    node.font_size, node_font,
                    line.c_str(), static_cast<unsigned int>(line.size()));
                float tx = draw_x + node.style.padding[3];
                if (node.style.text_align == TextAlign::Center)
                    tx += (inner_width - line_w) / 2;
                else if (node.style.text_align == TextAlign::End)
                    tx += inner_width - line_w;
                // Widget-tree text never rotates and uses the native
                // glyph advance; pass 0.0f rotation and 1.0f width
                // factor so the wire format stays consistent with
                // rotated / stretched canvas draws.
                emit_draw_text(r, tx, ty, node.font_size, /*rotation=*/0.0f,
                               /*width_factor=*/1.0f,
                               node_flags,
                               tc, node_font.family, line.c_str(),
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
            M const& measurer;
            float origin_x;
            float origin_y;
            float clip_x0, clip_y0, clip_x1, clip_y1;

            // Clip stack for nested push_clip / pop_clip. The bottom
            // entry is the canvas's outer rect (set in the
            // constructor); each push intersects with the current top
            // and emits `Cmd::Scissor` so macOS / Android backends
            // narrow their setScissorRect / vkCmdSetScissor between
            // draws. The CPU-side cull paths (line clip_line, arc bbox
            // reject, text bbox reject) read from clip_x0..clip_y1
            // which always tracks the top of the stack.
            struct ClipRect { float x0, y0, x1, y1; };
            std::vector<ClipRect> clip_stack;

            PainterImpl(R& r_in, M const& m_in, float ox, float oy,
                        float cx, float cy, float cw, float ch)
                : r(r_in), measurer(m_in),
                  origin_x(ox), origin_y(oy),
                  clip_x0(cx), clip_y0(cy),
                  clip_x1(cx + cw), clip_y1(cy + ch) {
                clip_stack.push_back({clip_x0, clip_y0, clip_x1, clip_y1});
            }

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

            void fill_path(PathBuilder const& path,
                           Color color) override {
                if (path.empty()) return;
                // Same translation pattern as `stroke_path`. The
                // backend ear-clips and rasterises into the existing
                // colour pipeline at decode time — no thickness here.
                auto translated = path.translated(origin_x, origin_y);
                emit_fill_path(r, translated, color);
            }

            void fill_quads(PaintQuad const* quads,
                            unsigned int count) override {
                if (!quads || count == 0) return;
                emit_fill_quads_translated(r, quads, count,
                                           origin_x, origin_y);
            }

            void fill_rects(PaintRect const* rects,
                            unsigned int count) override {
                if (!rects || count == 0) return;
                emit_fill_rects_translated(r, rects, count,
                                           origin_x, origin_y);
            }

            void text(float x, float y,
                      char const* str, unsigned int len,
                      float font_size, Color color,
                      FontSpec font = {},
                      float rotation = 0.0f) override {
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
                // Rotated runs may extend in any direction from the
                // pivot; widen the cull bbox to a circle of radius
                // `approx_w` (the longest extent under any rotation).
                // For axis-aligned runs the original tight bbox still
                // applies — early-out keeps offscreen text cheap.
                if (rotation == 0.0f) {
                    if (ax           >= clip_x1
                        || ax + approx_w <= clip_x0
                        || ay            >= clip_y1
                        || ay + approx_h <= clip_y0) {
                        return;
                    }
                } else {
                    float const rad = approx_w;
                    if (ax + rad <= clip_x0 || ax - rad >= clip_x1
                        || ay + rad <= clip_y0 || ay - rad >= clip_y1) {
                        return;
                    }
                }
                emit_draw_text(r,
                               origin_x + x, origin_y + y,
                               font_size, rotation, font.width_factor,
                               pack_font_flags(font),
                               color, font.family, str, len);
            }

            float measure_text(char const* str, unsigned int len,
                               float font_size,
                               FontSpec font = {}) const override {
                if (len == 0) return 0.0f;
                // Bypass measure_text_cached (layout's TU-private cache).
                // Painter consumers like cadpp drive their own
                // measurement loop and typically maintain their own
                // FontSpec→width cache; calling the host directly is
                // consistent with that pattern.
                //
                // FontSpec::width_factor scales each glyph's advance,
                // so multiply the host's natural-font measurement here
                // — the rendered glyphs land at proportionally wider /
                // narrower x positions and the cursor / centring math
                // upstream (canvas painters, MTEXT wrap decisions in
                // cadpp) stays in lockstep with what's drawn.
                float const natural =
                    measurer.measure_text(font_size, font, str, len);
                return natural * font.width_factor;
            }

            FontMetrics font_metrics(float font_size,
                                     FontSpec font = {}) const override {
                // Hosts without per-face metric resolution surface a
                // zero `FontMetrics{}` — the caller is expected to
                // fall back to a font-size-based heuristic in that
                // case. `width_factor` doesn't influence vertical
                // metrics, so we never multiply here.
                return measurer.font_metrics(font_size, font);
            }

            void push_clip(float x, float y,
                           float w, float h) override {
                // Translate the canvas-local rect to surface-local then
                // intersect with the current clip top so a nested push
                // never widens the visible region.
                float nx0 = origin_x + x;
                float ny0 = origin_y + y;
                float nx1 = nx0 + w;
                float ny1 = ny0 + h;
                ClipRect const& top = clip_stack.back();
                ClipRect inter{
                    std::max(top.x0, nx0),
                    std::max(top.y0, ny0),
                    std::min(top.x1, nx1),
                    std::min(top.y1, ny1),
                };
                clip_stack.push_back(inter);
                clip_x0 = inter.x0; clip_y0 = inter.y0;
                clip_x1 = inter.x1; clip_y1 = inter.y1;
                // Empty intersection still emits a degenerate scissor;
                // the backend's `vkCmdSetScissor` / `setScissorRect`
                // accepts (w == 0 || h == 0) as a "draw nothing" rect
                // which matches the CPU cull behaviour above.
                float ew = std::max(0.0f, inter.x1 - inter.x0);
                float eh = std::max(0.0f, inter.y1 - inter.y0);
                emit_scissor(r, inter.x0, inter.y0, ew, eh);
            }

            void pop_clip() override {
                // Defensive: a stray pop without a matching push would
                // underflow the stack and re-emit a (0,0,0,0) reset
                // that drops the canvas's own outer scissor for the
                // remainder of paint_fn. Keep the bottom entry pinned.
                if (clip_stack.size() <= 1) return;
                clip_stack.pop_back();
                ClipRect const& top = clip_stack.back();
                clip_x0 = top.x0; clip_y0 = top.y0;
                clip_x1 = top.x1; clip_y1 = top.y1;
                float ew = std::max(0.0f, top.x1 - top.x0);
                float eh = std::max(0.0f, top.y1 - top.y0);
                emit_scissor(r, top.x0, top.y0, ew, eh);
            }
        };

        // Use the scroll-adjusted `draw_x/y` (not the layout-frame
        // `ax/ay`) for both the wire-format scissor and the
        // PainterImpl's clip rect. Backends rasterise into the
        // framebuffer's surface space — `setScissorRect` /
        // `vkCmdSetScissor` need post-scroll pixel coords, and the
        // CPU-side cull paths must match the same space the painter
        // draws into (`origin_x/y == draw_x/y`). Earlier code passed
        // `ax/ay` here, which left the clip pinned to the canvas's
        // unscrolled position — once the user scrolled, the painter's
        // emit positions slid up by `scroll_y` while the clip stayed
        // at the canvas's original top, masking the top of the
        // drawing and leaking a horizontal band of "frozen" content.
        bool emit_canvas_scissor = (g_app.paint_scissor_depth == 0);
        if (emit_canvas_scissor) {
            emit_scissor(r, draw_x, draw_y, node.width, node.height);
            ++g_app.paint_scissor_depth;
            metrics::inst::scissor_emitted.add();
        }
        PainterImpl painter(r, measurer, draw_x, draw_y,
                            draw_x, draw_y,
                            node.width, node.height);
        node.paint_fn(painter);
        if (emit_canvas_scissor) {
            emit_scissor_reset(r);
            --g_app.paint_scissor_depth;
        }
    }

    if (node.callback_id != 0xFFFFFFFF) {
        // hit_test compares the cursor against HitRegion rects after
        // adding the *global* scroll back, so a child of a scroll_view
        // whose region was emitted at raw layout coords would miss
        // when its container is scrolled. Subtract just the *inner*
        // scroll (ambient minus global) so the rect lines up with the
        // visual position once hit_test re-applies global. Root
        // children always have inner == 0 and so emit at the
        // unchanged (ax, ay).
        float inner_scroll_x = scroll_x - g_app.scroll_x;
        float inner_scroll_y = scroll_y - g_app.scroll_y;
        emit_hit_region(r, ax - inner_scroll_x, ay - inner_scroll_y,
                        node.width, node.height,
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

    // Scroll viewport setup. Clamp the offset against this frame's
    // measured content_height so a layout shrink (or a wheel write
    // that didn't see the latest content) can't leave the viewport
    // pointing past its content. The clamped value also feeds the
    // child-scroll passed into the recursion below; we write back
    // through `node.scroll_state` so the next wheel event reads from
    // the same clamped value.
    float child_scroll_x = scroll_x;
    float child_scroll_y = scroll_y;
    bool emit_scroll_scissor = false;
    if (node.is_scroll_container && node.scroll_state) {
        float max_off = std::max(0.0f, node.content_height - node.height);
        float& off = node.scroll_state->offset_y;
        if (off < 0.0f) off = 0.0f;
        if (off > max_off) off = max_off;
        node.scroll_offset_y = off;
        child_scroll_y = scroll_y + off;

        AppState::ScrollTarget tgt;
        tgt.x = ax - scroll_x;
        tgt.y = ay - scroll_y;
        tgt.w = node.width;
        tgt.h = node.height;
        tgt.state = node.scroll_state;
        tgt.content_height = node.content_height;
        g_app.scroll_targets.push_back(tgt);

        // Scissor the viewport so children that scroll past the rect
        // get clipped. Skip when already nested in another scissor —
        // backends can't stack regions; the outer one wins. Inner
        // scroll_views that lose the scissor still scroll correctly,
        // they just visually bleed past the inner viewport's edges
        // (rare in practice; documented limitation).
        if (g_app.paint_scissor_depth == 0) {
            emit_scissor(r, ax - scroll_x, ay - scroll_y,
                         node.width, node.height);
            ++g_app.paint_scissor_depth;
            emit_scroll_scissor = true;
            metrics::inst::scissor_emitted.add();
        }
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
            float cx = ax + child.x - child_scroll_x;
            float cy = ay + child.y - child_scroll_y;
            emit_scissor(r, cx, cy, child.width, child.height);
            ++g_app.paint_scissor_depth;
            metrics::inst::scissor_emitted.add();
        }

        paint_node(r, measurer, child_h, ax, ay,
                   child_scroll_x, child_scroll_y, vp_width, vp_height,
                   effective_inside);
        subtree_mask |= node_at(child_h).paint_callback_mask;
        subtree_dynamic = subtree_dynamic || node_at(child_h).paint_dynamic;

        if (child_is_dirty_root) {
            emit_scissor_reset(r);
            --g_app.paint_scissor_depth;
        }
    }

    if (emit_scroll_scissor) {
        emit_scissor_reset(r);
        --g_app.paint_scissor_depth;
    }

    // Record paint-cache state for the next frame's diff to copy and
    // the next frame's paint_node entry guard to consult. If ensure()
    // had to flush mid-walk (buf_len reset to 0), the recorded offset
    // would be meaningless; skip caching in that case. Comparing
    // paint_flush_epoch against entry_flush_epoch catches the case
    // where a mid-walk flush was followed by enough re-emit to make
    // after >= before numerically, which the `after >= before` check
    // alone would silently accept as valid.
    //
    // Subtrees inside (or rooting) a scroll_view never record paint
    // state — the cache invariant doesn't track per-node ambient
    // scroll, so a blit on a later frame with a different scroll
    // offset would render stale draw positions.
    auto const after = r.buf_len();
    node.paint_dynamic = subtree_dynamic;
    if (!effective_inside
        && !subtree_dynamic && after >= before
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
inline void wasi_reset_paint_scissor_boundary() {
    reset_paint_scissor_boundary(g_wasi);
}
inline void wasi_paint_node(NodeHandle h, float ox, float oy,
                            float sx, float sy, float vw, float vh) {
    paint_node(g_wasi, g_wasi, h, ox, oy, sx, sy, vw, vh);
}
}
#endif

module;
#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>
#include <vector>
#endif

export module phenotype.native.macos.commands;

#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
import phenotype.commands;
import phenotype.material;
import phenotype.native.macos.material;
import phenotype.native.macos.render;
import phenotype.state;
import phenotype.types;

export namespace phenotype::native::detail {

inline Color unpack_color(unsigned int packed) noexcept {
    return {
        static_cast<unsigned char>((packed >> 24) & 0xFF),
        static_cast<unsigned char>((packed >> 16) & 0xFF),
        static_cast<unsigned char>((packed >>  8) & 0xFF),
        static_cast<unsigned char>( packed        & 0xFF),
    };
}

inline void append_color_instance(std::vector<ColorInstanceGPU>& out,
                                  float x, float y, float w, float h,
                                  float r, float g, float b, float a,
                                  float p0 = 0.0f, float p1 = 0.0f,
                                  float p2 = 0.0f, float p3 = 0.0f) {
    ColorInstanceGPU inst{};
    inst.rect[0] = x;
    inst.rect[1] = y;
    inst.rect[2] = w;
    inst.rect[3] = h;
    inst.color[0] = r;
    inst.color[1] = g;
    inst.color[2] = b;
    inst.color[3] = a;
    inst.params[0] = p0;
    inst.params[1] = p1;
    inst.params[2] = p2;
    inst.params[3] = p3;
    out.push_back(inst);
}

inline void append_tri_vertex(std::vector<TriVertexGPU>& out,
                              float x, float y,
                              float r, float g, float b, float a) {
    TriVertexGPU v;
    v.pos[0] = x;
    v.pos[1] = y;
    v.color[0] = r;
    v.color[1] = g;
    v.color[2] = b;
    v.color[3] = a;
    out.push_back(v);
}

inline bool stroked_segment_needs_triangle_body(float x1,
                                                float y1,
                                                float x2,
                                                float y2,
                                                float thickness) noexcept {
    if (thickness <= 0.0f)
        return false;
    float const dx = x2 - x1;
    float const dy = y2 - y1;
    float const line_len = std::sqrt(dx * dx + dy * dy);
    if (line_len <= 0.0f)
        return false;
    return std::fabs(dx) > 0.0001f && std::fabs(dy) > 0.0001f;
}

inline void append_stroked_segment_to_batch(ScissorBatch& batch,
                                            float x1, float y1,
                                            float x2, float y2,
                                            float thickness,
                                            float r, float g,
                                            float b, float a) {
    float const dx = x2 - x1;
    float const dy = y2 - y1;
    float const line_len = std::sqrt(dx * dx + dy * dy);
    if (line_len <= 0.0f || thickness <= 0.0f)
        return;

    float const half_th = thickness * 0.5f;
    if (!stroked_segment_needs_triangle_body(x1, y1, x2, y2, thickness)) {
        float const w = (std::fabs(dy) <= 0.0001f) ? line_len : thickness;
        float const h = (std::fabs(dx) <= 0.0001f) ? line_len : thickness;
        float const x = (std::fabs(dx) <= 0.0001f)
            ? x1 - half_th
            : (x1 < x2 ? x1 : x2);
        float const y = (std::fabs(dy) <= 0.0001f)
            ? y1 - half_th
            : (y1 < y2 ? y1 : y2);
        append_color_instance(
            batch.colors,
            x, y, w, h, r, g, b, a,
            half_th, 0.0f, 2.0f);
        return;
    }

    float const nx = -dy / line_len * half_th;
    float const ny = dx / line_len * half_th;
    auto& tris = batch.tri_vertices;
    append_tri_vertex(tris, x1 + nx, y1 + ny, r, g, b, a);
    append_tri_vertex(tris, x2 + nx, y2 + ny, r, g, b, a);
    append_tri_vertex(tris, x2 - nx, y2 - ny, r, g, b, a);
    append_tri_vertex(tris, x1 + nx, y1 + ny, r, g, b, a);
    append_tri_vertex(tris, x2 - nx, y2 - ny, r, g, b, a);
    append_tri_vertex(tris, x1 - nx, y1 - ny, r, g, b, a);

    append_color_instance(
        batch.colors,
        x1 - half_th, y1 - half_th, thickness, thickness,
        r, g, b, a, half_th, 0.0f, 2.0f);
    append_color_instance(
        batch.colors,
        x2 - half_th, y2 - half_th, thickness, thickness,
        r, g, b, a, half_th, 0.0f, 2.0f);
}

inline void append_stroked_segment(FrameScratch& scratch,
                                   float x1, float y1,
                                   float x2, float y2,
                                   float thickness,
                                   float r, float g,
                                   float b, float a) {
    if (stroked_segment_needs_triangle_body(x1, y1, x2, y2, thickness))
        ensure_triangle_order_batch(scratch);
    append_stroked_segment_to_batch(
        scratch.batches.back(),
        x1, y1, x2, y2,
        thickness,
        r, g, b, a);
}

inline void append_linear_gradient_instances(
        std::vector<ColorInstanceGPU>& out,
        float x, float y, float w, float h,
        Color from, Color to, GradientAxis axis, unsigned int steps) {
    if (w == 0.0f || h == 0.0f)
        return;
    if (w < 0.0f) { x += w; w = -w; }
    if (h < 0.0f) { y += h; h = -h; }

    unsigned int const count = linear_gradient_step_count(steps);
    out.reserve(out.size() + count);
    for (unsigned int i = 0; i < count; ++i) {
        float const t = count == 1
            ? 0.0f
            : static_cast<float>(i) / static_cast<float>(count - 1);
        auto const color = lerp_color(from, to, t);
        if (axis == GradientAxis::Horizontal) {
            float const x0 = x + w * static_cast<float>(i)
                / static_cast<float>(count);
            float const x1 = x + w * static_cast<float>(i + 1)
                / static_cast<float>(count);
            append_color_instance(
                out,
                x0,
                y,
                x1 - x0,
                h,
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
                color.a / 255.0f);
        } else {
            float const y0 = y + h * static_cast<float>(i)
                / static_cast<float>(count);
            float const y1 = y + h * static_cast<float>(i + 1)
                / static_cast<float>(count);
            append_color_instance(
                out,
                x,
                y0,
                w,
                y1 - y0,
                color.r / 255.0f,
                color.g / 255.0f,
                color.b / 255.0f,
                color.a / 255.0f);
        }
    }
}

// FillPath helpers. Walk a flat polygon (vertex list with an implicit
// close) and ear-clip it into a triangle list. The triangles are then
// fed into the dedicated triangle pipeline (`vs_tri` / `fs_tri`) as
// raw 3-vertex tuples — hardware rasterisation gives pixel-perfect
// coverage on shared edges and collapses tens of thousands of HATCH
// fills (e.g. colorwh.dwg's 36 095 boundary loops) into a single
// drawPrimitives call.
//
// The earlier CPU scanline path emitted one ColorInstance per
// 1-pixel-tall horizontal strip per triangle, which both (a) produced
// sub-pixel gaps at slim hatch tips when the strip width fell below
// 0.5 px and the strip was dropped, and (b) ballooned the colour
// instance buffer past tens of MB on dense CAD content.
inline bool point_in_triangle(float px, float py,
                              float ax, float ay,
                              float bx, float by,
                              float cx, float cy) {
    // Sign-of-cross-products test. CCW polygon = all three crosses
    // share the same sign for an interior point.
    float const d1 = (px - bx) * (ay - by) - (ax - bx) * (py - by);
    float const d2 = (px - cx) * (by - cy) - (bx - cx) * (py - cy);
    float const d3 = (px - ax) * (cy - ay) - (cx - ax) * (py - ay);
    bool const has_neg = (d1 < 0) || (d2 < 0) || (d3 < 0);
    bool const has_pos = (d1 > 0) || (d2 > 0) || (d3 > 0);
    return !(has_neg && has_pos);
}

// Ear-clip triangulation. Writes 6 floats per triangle (v0x, v0y,
// v1x, v1y, v2x, v2y) appended to `tris`; the caller is responsible
// for any reset / size-tracking. `poly` is taken by reference and
// MAY be reordered in place (orientation normalisation). `remain` is
// caller-owned scratch space — passing it in avoids a per-call
// std::vector allocation that becomes 36k+ allocs/frame on
// HATCH-heavy DWGs (colorwh.dwg).
inline void polygon_ear_clip(std::vector<float>& poly,
                             std::vector<float>& tris,
                             std::vector<std::size_t>& remain) {
    if (poly.size() < 6) return;  // need ≥ 3 vertices

    std::size_t const n0 = poly.size() / 2;

    // Signed area × 2 to detect orientation. CCW > 0; CW < 0.
    double area2 = 0.0;
    for (std::size_t i = 0; i < n0; ++i) {
        std::size_t const j = (i + 1) % n0;
        area2 += static_cast<double>(poly[2 * i])     * poly[2 * j + 1]
              -  static_cast<double>(poly[2 * j])     * poly[2 * i + 1];
    }
    if (std::fabs(area2) < 1e-9) return;

    // Normalise to CCW so the convex-vertex test below has a stable sign.
    if (area2 < 0.0) {
        for (std::size_t i = 0; i < n0 / 2; ++i) {
            std::swap(poly[2 * i],     poly[2 * (n0 - 1 - i)]);
            std::swap(poly[2 * i + 1], poly[2 * (n0 - 1 - i) + 1]);
        }
    }

    // Indices into `poly` (vertex indices, not float offsets).
    remain.clear();
    remain.reserve(n0);
    for (std::size_t i = 0; i < n0; ++i) remain.push_back(i);

    // Convex fast path: triangle-fan from vertex 0. Most CAD HATCH
    // boundaries (wedges, rectangles, regular polygons) are convex,
    // and the dominant colorwh.dwg case fits this. Skip the
    // O(N²) ear search when no reflex vertex exists.
    bool convex = true;
    for (std::size_t i = 0; i < n0 && convex; ++i) {
        std::size_t const ip = (i + n0 - 1) % n0;
        std::size_t const ic = i;
        std::size_t const in = (i + 1) % n0;
        float const ax = poly[2 * ip], ay = poly[2 * ip + 1];
        float const bx = poly[2 * ic], by = poly[2 * ic + 1];
        float const cx = poly[2 * in], cy = poly[2 * in + 1];
        float const cross = (bx - ax) * (cy - ay)
                          - (by - ay) * (cx - ax);
        if (cross < 0.0f) convex = false;
    }
    if (convex) {
        float const ax = poly[0], ay = poly[1];
        for (std::size_t i = 1; i + 1 < n0; ++i) {
            float const bx = poly[2 * i],     by = poly[2 * i + 1];
            float const cx = poly[2 * (i+1)], cy = poly[2 * (i+1) + 1];
            tris.push_back(ax); tris.push_back(ay);
            tris.push_back(bx); tris.push_back(by);
            tris.push_back(cx); tris.push_back(cy);
        }
        return;
    }

    // Safety bound: at most n triangles for n vertices, but loop more
    // generously to tolerate near-degenerate ears.
    int safety = static_cast<int>(n0) * 3 + 1;
    while (remain.size() >= 3 && safety-- > 0) {
        bool found = false;
        std::size_t const m = remain.size();
        for (std::size_t i = 0; i < m; ++i) {
            std::size_t const ip = remain[(i + m - 1) % m];
            std::size_t const ic = remain[i];
            std::size_t const in = remain[(i + 1) % m];
            float const ax = poly[2 * ip], ay = poly[2 * ip + 1];
            float const bx = poly[2 * ic], by = poly[2 * ic + 1];
            float const cx = poly[2 * in], cy = poly[2 * in + 1];
            // Convex (CCW) test: cross > 0.
            float const cross = (bx - ax) * (cy - ay)
                              - (by - ay) * (cx - ax);
            if (cross <= 0.0f) continue;  // reflex / colinear
            // Empty test: no other remaining vertex strictly inside abc.
            bool empty = true;
            for (std::size_t k : remain) {
                if (k == ip || k == ic || k == in) continue;
                float const px = poly[2 * k], py = poly[2 * k + 1];
                if (point_in_triangle(px, py, ax, ay, bx, by, cx, cy)) {
                    empty = false; break;
                }
            }
            if (!empty) continue;
            tris.push_back(ax); tris.push_back(ay);
            tris.push_back(bx); tris.push_back(by);
            tris.push_back(cx); tris.push_back(cy);
            remain.erase(remain.begin() + static_cast<long>(i));
            found = true;
            break;
        }
        if (!found) break;  // self-intersecting / degenerate polygon
    }
}


inline void append_text_instance(std::vector<TextInstanceGPU>& out,
                                 float x, float y, float w, float h,
                                 float u, float v, float uw, float vh,
                                 float r, float g, float b, float a,
                                 float pivot_x = 0.0f, float pivot_y = 0.0f,
                                 float cos_t = 1.0f, float sin_t = 0.0f) {
    TextInstanceGPU inst{};
    inst.rect[0] = x;
    inst.rect[1] = y;
    inst.rect[2] = w;
    inst.rect[3] = h;
    inst.uv_rect[0] = u;
    inst.uv_rect[1] = v;
    inst.uv_rect[2] = uw;
    inst.uv_rect[3] = vh;
    inst.color[0] = r;
    inst.color[1] = g;
    inst.color[2] = b;
    inst.color[3] = a;
    inst.rot[0] = pivot_x;
    inst.rot[1] = pivot_y;
    inst.rot[2] = cos_t;
    inst.rot[3] = sin_t;
    out.push_back(inst);
}

inline void append_image_instance(std::vector<ImageInstanceGPU>& out,
                                  float x, float y, float w, float h,
                                  float u, float v, float uw, float vh,
                                  float r = 1.0f, float g = 1.0f,
                                  float b = 1.0f, float a = 1.0f) {
    ImageInstanceGPU inst{};
    inst.rect[0] = x;
    inst.rect[1] = y;
    inst.rect[2] = w;
    inst.rect[3] = h;
    inst.uv_rect[0] = u;
    inst.uv_rect[1] = v;
    inst.uv_rect[2] = uw;
    inst.uv_rect[3] = vh;
    inst.color[0] = r;
    inst.color[1] = g;
    inst.color[2] = b;
    inst.color[3] = a;
    out.push_back(inst);
}

struct CommandReader {
    unsigned char const* cur = nullptr;
    unsigned char const* end = nullptr;

    bool can_read(unsigned int bytes) const noexcept {
        return cur + bytes <= end;
    }

    bool read_u32(unsigned int& out) noexcept {
        if (!can_read(4)) return false;
        std::memcpy(&out, cur, 4);
        cur += 4;
        return true;
    }

    bool read_f32(float& out) noexcept {
        unsigned int bits = 0;
        if (!read_u32(bits)) return false;
        std::memcpy(&out, &bits, 4);
        return true;
    }

    bool read_text(char const*& data, unsigned int len) noexcept {
        if (!can_read(len)) return false;
        data = reinterpret_cast<char const*>(cur);
        cur += len;
        auto const padded_len = (len + 3u) & ~3u;
        if (padded_len > len) {
            if (!can_read(padded_len - len)) return false;
            cur += padded_len - len;
        }
        return true;
    }
};

inline bool fills_logical_viewport(float x, float y, float w, float h,
                                   MaterialEnvironment const& env) noexcept {
    float const scale = env.render_target.scale > 0.0f
        ? env.render_target.scale
        : 1.0f;
    float const logical_width =
        static_cast<float>(env.render_target.width) / scale;
    float const logical_height =
        static_cast<float>(env.render_target.height) / scale;
    constexpr float tolerance = 0.75f;
    return std::fabs(x) <= tolerance
        && std::fabs(y) <= tolerance
        && std::fabs(w - logical_width) <= tolerance
        && std::fabs(h - logical_height) <= tolerance;
}

inline bool decode_frame_commands(unsigned char const* buf, unsigned int len,
                                  float line_height_ratio,
                                  MaterialEnvironment const& material_env,
                                  FrameScratch& scratch,
                                  std::vector<HitRegionCmd>& hit_regions,
                                  double& cr, double& cg,
                                  double& cb, double& ca) {
    scratch.clear();
    hit_regions.clear();
    cr = 0.98;
    cg = 0.98;
    cb = 0.98;
    ca = 1.0;

    CommandReader reader{buf, buf + len};
    std::uint32_t command_index = 0;
    while (reader.cur < reader.end) {
        unsigned int raw_cmd = 0;
        if (!reader.read_u32(raw_cmd))
            return false;
        auto const current_command_index = command_index++;

        switch (static_cast<Cmd>(raw_cmd)) {
            case Cmd::Clear: {
                unsigned int packed = 0;
                if (!reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                cr = color.r / 255.0;
                cg = color.g / 255.0;
                cb = color.b / 255.0;
                ca = color.a / 255.0;
                break;
            }
            case Cmd::FillRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                if (color.a == 255
                    && fills_logical_viewport(x, y, w, h, material_env)) {
                    ++scratch.full_frame_opaque_fill_count;
                }
                append_color_instance(
                    scratch.batches.back().colors,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f);
                break;
            }
            case Cmd::StrokeRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, line_width = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_f32(line_width)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_color_instance(
                    scratch.batches.back().colors,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    0.0f, line_width, 1.0f);
                break;
            }
            case Cmd::RoundRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, radius = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_f32(radius)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_color_instance(
                    scratch.batches.back().colors,
                    x, y, w, h,
                    color.r / 255.0f, color.g / 255.0f,
                    color.b / 255.0f, color.a / 255.0f,
                    radius, 0.0f, 2.0f);
                break;
            }
            case Cmd::MaterialRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                float radius = 0.0f;
                unsigned int kind = 0;
                unsigned int role = 0;
                unsigned int allows_liquid_glass = 1u;
                float opacity = 0.0f;
                float blur_radius = 0.0f;
                unsigned int packed = 0;
                float saturation = 1.0f;
                float luminance_floor = 0.0f;
                float luminance_gain = 1.0f;
                float edge_highlight = 0.0f;
                float edge_width = 1.0f;
                float noise_opacity = 0.0f;
                float shadow_alpha = 0.0f;
                float shadow_radius = 0.0f;
                unsigned int container_id = 0;
                unsigned int union_id = 0;
                float container_spacing = 0.0f;
                unsigned int container_flags = 0;
                unsigned int interaction_flags = 0;
                float interaction_x = 0.5f;
                float interaction_y = 0.5f;
                unsigned int transition_kind = 0;
                float transition_progress = 1.0f;
                unsigned int transition_flags = 1u;
                unsigned int glass_namespace_id = 0;
                unsigned int glass_effect_id = 0;
                unsigned int glass_background_kind = 0;
                float glass_background_feather_padding = 0.0f;
                float glass_background_soft_edge_radius = 0.0f;
                unsigned int prominence_flags = 0;
                float prominence_intensity = 1.0f;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_f32(radius)
                    || !reader.read_u32(kind)
                    || !reader.read_u32(role)
                    || !reader.read_u32(allows_liquid_glass)
                    || !reader.read_f32(opacity)
                    || !reader.read_f32(blur_radius)
                    || !reader.read_u32(packed)
                    || !reader.read_f32(saturation)
                    || !reader.read_f32(luminance_floor)
                    || !reader.read_f32(luminance_gain)
                    || !reader.read_f32(edge_highlight)
                    || !reader.read_f32(edge_width)
                    || !reader.read_f32(noise_opacity)
                    || !reader.read_f32(shadow_alpha)
                    || !reader.read_f32(shadow_radius)
                    || !reader.read_u32(container_id)
                    || !reader.read_u32(union_id)
                    || !reader.read_f32(container_spacing)
                    || !reader.read_u32(container_flags)
                    || !reader.read_u32(interaction_flags)
                    || !reader.read_f32(interaction_x)
                    || !reader.read_f32(interaction_y)
                    || !reader.read_u32(transition_kind)
                    || !reader.read_f32(transition_progress)
                    || !reader.read_u32(transition_flags)
                    || !reader.read_u32(glass_namespace_id)
                    || !reader.read_u32(glass_effect_id)
                    || !reader.read_u32(glass_background_kind)
                    || !reader.read_f32(glass_background_feather_padding)
                    || !reader.read_f32(glass_background_soft_edge_radius)
                    || !reader.read_u32(prominence_flags)
                    || !reader.read_f32(prominence_intensity))
                    return false;
                auto material_env_for_command = material_env;
                material_env_for_command.debug_seed.node =
                    current_command_index;
                MaterialCommandDescriptor descriptor{
                    material_kind_from_wire(kind),
                    material_surface_role_from_wire(role),
                    allows_liquid_glass != 0u,
                    material_container_descriptor_from_wire(
                        container_id,
                        union_id,
                        container_spacing,
                        container_flags),
                    opacity,
                    blur_radius,
                    unpack_color(packed),
                    saturation,
                    luminance_floor,
                    luminance_gain,
                    edge_highlight,
                    edge_width,
                    noise_opacity,
                    shadow_alpha,
                    shadow_radius,
                    material_interaction_descriptor_from_wire(
                        interaction_flags,
                        interaction_x,
                        interaction_y),
                    material_transition_descriptor_from_wire(
                        transition_kind,
                        transition_progress,
                        transition_flags),
                    material_glass_identity_from_wire(
                        glass_namespace_id,
                        glass_effect_id),
                    material_glass_background_from_wire(
                        glass_background_kind,
                        glass_background_feather_padding,
                        glass_background_soft_edge_radius),
                    material_prominence_from_wire(
                        prominence_flags,
                        prominence_intensity)};
                auto plan = plan_material_surface(
                    material_request_for_command(
                        descriptor,
                        MaterialGeometry{x, y, w, h, radius},
                        ::phenotype::detail::g_app.theme),
                    material_env_for_command);
                scratch.material_records.push_back(
                    MaterialRuntimeRecord{plan, current_command_index});
                auto& cur = scratch.batches.back();
                if (!cur.tri_vertices.empty() || !cur.colors.empty()
                    || !cur.materials.empty() || !cur.arcs.empty()
                    || !cur.images.empty() || !cur.texts.empty()
                    || cur.pending_text_runs) {
                    open_same_scissor_batch(scratch);
                }
                if (material_plan_uses_sampled_backdrop_executor(plan)) {
                    append_material_instance(
                        scratch.batches.back().materials,
                        plan,
                        current_command_index);
                } else {
                    auto const batch_index = scratch.batches.size() - 1u;
                    auto& colors = scratch.batches.back().colors;
                    auto const first = colors.size();
                    append_material_paint_layer_instances(
                        colors,
                        plan);
                    auto const count = colors.size() - first;
                    if (count > 0u) {
                        scratch.material_paint_layer_ranges.push_back(
                            MaterialPaintLayerRange{
                                current_command_index,
                                batch_index,
                                first,
                                count});
                    }
                }
                open_same_scissor_batch(scratch);
                break;
            }
            case Cmd::DrawText: {
                float x = 0.0f, y = 0.0f, font_size = 0.0f, rotation = 0.0f;
                float width_factor = 1.0f;
                unsigned int flags = 0;
                unsigned int packed = 0;
                unsigned int family_len = 0;
                unsigned int text_len = 0;
                char const* family = nullptr;
                char const* text = nullptr;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(font_size)
                    || !reader.read_f32(rotation)
                    || !reader.read_f32(width_factor)
                    || !reader.read_u32(flags)
                    || !reader.read_u32(packed) || !reader.read_u32(family_len)
                    || !reader.read_text(family, family_len)
                    || !reader.read_u32(text_len)
                    || !reader.read_text(text, text_len))
                    return false;
                auto color = unpack_color(packed);
                auto foreground = material_resolve_text_foreground(
                    scratch.material_records,
                    current_command_index,
                    x,
                    y,
                    color,
                    ::phenotype::detail::g_app.theme);
                if (foreground.has_material)
                    ++scratch.foreground_text_candidate_count;
                if (foreground.remapped) {
                    ++scratch.foreground_text_remap_count;
                    color = foreground.color;
                }
                ParsedTextRun run{};
                run.x = x;
                run.y = y;
                run.font_size = font_size;
                run.rotation = rotation;
                run.width_factor = width_factor;
                run.mono = (flags & 1u) != 0;
                run.r = color.r / 255.0f;
                run.g = color.g / 255.0f;
                run.b = color.b / 255.0f;
                run.a = color.a / 255.0f;
                run.line_height = font_size * line_height_ratio;
                run.text = text;
                run.len = text_len;
                run.batch_idx = static_cast<std::uint32_t>(scratch.batches.size() - 1);
                run.command_index = current_command_index;
                if (family && family_len > 0)
                    run.font_key.family.assign(family, family_len);
                run.font_key.weight = (flags & 2u)
                    ? ::phenotype::FontWeight::Bold
                    : ::phenotype::FontWeight::Regular;
                run.font_key.style  = (flags & 4u)
                    ? ::phenotype::FontStyle::Italic
                    : ::phenotype::FontStyle::Upright;
                run.font_key.mono = run.mono;
                scratch.batches.back().pending_text_runs = true;
                scratch.text_runs.push_back(std::move(run));
                break;
            }
            case Cmd::DrawLine: {
                float x1 = 0.0f, y1 = 0.0f, x2 = 0.0f, y2 = 0.0f, thickness = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(x1) || !reader.read_f32(y1)
                    || !reader.read_f32(x2) || !reader.read_f32(y2)
                    || !reader.read_f32(thickness)
                    || !reader.read_u32(packed))
                    return false;
                auto color = unpack_color(packed);
                append_stroked_segment(
                    scratch,
                    x1, y1, x2, y2, thickness,
                    color.r / 255.0f,
                    color.g / 255.0f,
                    color.b / 255.0f,
                    color.a / 255.0f);
                break;
            }
            case Cmd::HitRegion: {
                HitRegionCmd hit{};
                unsigned int cursor_type = 0;
                if (!reader.read_f32(hit.x) || !reader.read_f32(hit.y)
                    || !reader.read_f32(hit.w) || !reader.read_f32(hit.h)
                    || !reader.read_u32(hit.callback_id)
                    || !reader.read_u32(cursor_type))
                    return false;
                hit.cursor_type = cursor_type;
                hit_regions.push_back(hit);
                break;
            }
            case Cmd::DrawImage: {
                PendingImageCmd image{};
                unsigned int url_len = 0;
                char const* url = nullptr;
                if (!reader.read_f32(image.x) || !reader.read_f32(image.y)
                    || !reader.read_f32(image.w) || !reader.read_f32(image.h)
                    || !reader.read_u32(url_len)
                    || !reader.read_text(url, url_len))
                    return false;
                image.url.assign(url ? url : "", url_len);
                image.batch_idx =
                    static_cast<std::uint32_t>(scratch.batches.size() - 1);
                scratch.images.push_back(std::move(image));
                break;
            }
            case Cmd::Scissor: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h))
                    return false;
                open_scissor_batch(scratch, x, y, w, h);
                break;
            }
            case Cmd::DrawArc: {
                float cx = 0.0f, cy = 0.0f, r = 0.0f;
                float sa = 0.0f, ea = 0.0f, th = 0.0f;
                unsigned int packed = 0;
                if (!reader.read_f32(cx) || !reader.read_f32(cy)
                    || !reader.read_f32(r)
                    || !reader.read_f32(sa) || !reader.read_f32(ea)
                    || !reader.read_f32(th)
                    || !reader.read_u32(packed))
                    return false;
                if (r <= 0.0f) break;
                auto color = unpack_color(packed);
                ArcInstanceGPU inst{};
                inst.center_radius_thickness[0] = cx;
                inst.center_radius_thickness[1] = cy;
                inst.center_radius_thickness[2] = r;
                inst.center_radius_thickness[3] = th;
                inst.angles[0] = sa;
                inst.angles[1] = ea;
                inst.color[0]  = color.r / 255.0f;
                inst.color[1]  = color.g / 255.0f;
                inst.color[2]  = color.b / 255.0f;
                inst.color[3]  = color.a / 255.0f;
                scratch.batches.back().arcs.push_back(inst);
                break;
            }
            case Cmd::Path: {
                // Walk the verb stream and dispatch each segment onto
                // the existing instance buffers. Axis-aligned strokes
                // use the color SDF path, diagonal strokes use triangle
                // bodies plus round color caps, and ArcTo pushes an
                // `ArcInstanceGPU` directly. No new pipeline is introduced;
                // stroke rendering reuses the color, triangle, and arc
                // backends already in flight.
                float thickness = 0.0f;
                unsigned int packed = 0;
                unsigned int verb_count = 0;
                if (!reader.read_f32(thickness)
                    || !reader.read_u32(packed)
                    || !reader.read_u32(verb_count))
                    return false;
                auto color = unpack_color(packed);
                float const cr = color.r / 255.0f;
                float const cg = color.g / 255.0f;
                float const cb = color.b / 255.0f;
                float const ca = color.a / 255.0f;

                float pen_x = 0.0f, pen_y = 0.0f;
                float sub_x = 0.0f, sub_y = 0.0f;
                bool has_pen = false;

                bool triangle_stroke_batch_ready = false;
                auto emit_segment = [&](float x1, float y1,
                                        float x2, float y2) {
                    if (stroked_segment_needs_triangle_body(
                            x1, y1, x2, y2, thickness)) {
                        if (!triangle_stroke_batch_ready) {
                            ensure_triangle_order_batch(scratch);
                            triangle_stroke_batch_ready = true;
                        }
                    }
                    append_stroked_segment_to_batch(
                        scratch.batches.back(),
                        x1, y1, x2, y2,
                        thickness,
                        cr, cg, cb, ca);
                };

                // Recursive De Casteljau flatten — split until each
                // control point sits within `flatness = 0.5px` of the
                // chord, or until depth runs out (cap at 16 levels =
                // up to 65 536 segments per curve, which is far more
                // than any visible CAD input requires).
                constexpr float flatness2 = 0.25f;  // (0.5px)^2
                constexpr int max_depth = 16;

                auto flatten_quad = [&](auto& self,
                                        float p0x, float p0y,
                                        float p1x, float p1y,
                                        float p2x, float p2y,
                                        int depth) -> void {
                    float lx = p2x - p0x;
                    float ly = p2y - p0y;
                    float llen2 = lx * lx + ly * ly;
                    bool flat = (depth == 0);
                    if (!flat && llen2 > 1e-6f) {
                        float d = (p1x - p0x) * ly - (p1y - p0y) * lx;
                        if (d * d < flatness2 * llen2) flat = true;
                    }
                    if (flat || llen2 < 1e-6f) {
                        emit_segment(p0x, p0y, p2x, p2y);
                        return;
                    }
                    float a0x = (p0x + p1x) * 0.5f;
                    float a0y = (p0y + p1y) * 0.5f;
                    float a1x = (p1x + p2x) * 0.5f;
                    float a1y = (p1y + p2y) * 0.5f;
                    float midx = (a0x + a1x) * 0.5f;
                    float midy = (a0y + a1y) * 0.5f;
                    self(self, p0x, p0y, a0x, a0y, midx, midy, depth - 1);
                    self(self, midx, midy, a1x, a1y, p2x, p2y, depth - 1);
                };

                auto flatten_cubic = [&](auto& self,
                                         float p0x, float p0y,
                                         float p1x, float p1y,
                                         float p2x, float p2y,
                                         float p3x, float p3y,
                                         int depth) -> void {
                    float lx = p3x - p0x;
                    float ly = p3y - p0y;
                    float llen2 = lx * lx + ly * ly;
                    bool flat = (depth == 0);
                    if (!flat && llen2 > 1e-6f) {
                        float d1 = (p1x - p0x) * ly - (p1y - p0y) * lx;
                        float d2 = (p2x - p0x) * ly - (p2y - p0y) * lx;
                        if (d1 * d1 < flatness2 * llen2
                            && d2 * d2 < flatness2 * llen2) {
                            flat = true;
                        }
                    }
                    if (flat || llen2 < 1e-6f) {
                        emit_segment(p0x, p0y, p3x, p3y);
                        return;
                    }
                    float a01x = (p0x + p1x) * 0.5f;
                    float a01y = (p0y + p1y) * 0.5f;
                    float a12x = (p1x + p2x) * 0.5f;
                    float a12y = (p1y + p2y) * 0.5f;
                    float a23x = (p2x + p3x) * 0.5f;
                    float a23y = (p2y + p3y) * 0.5f;
                    float b01x = (a01x + a12x) * 0.5f;
                    float b01y = (a01y + a12y) * 0.5f;
                    float b12x = (a12x + a23x) * 0.5f;
                    float b12y = (a12y + a23y) * 0.5f;
                    float midx = (b01x + b12x) * 0.5f;
                    float midy = (b01y + b12y) * 0.5f;
                    self(self,
                         p0x, p0y, a01x, a01y, b01x, b01y, midx, midy,
                         depth - 1);
                    self(self,
                         midx, midy, b12x, b12y, a23x, a23y, p3x, p3y,
                         depth - 1);
                };

                for (unsigned int i = 0; i < verb_count; ++i) {
                    unsigned int verb = 0;
                    if (!reader.read_u32(verb)) return false;
                    switch (static_cast<PathVerb>(verb)) {
                    case PathVerb::MoveTo: {
                        float x = 0.0f, y = 0.0f;
                        if (!reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        pen_x = x; pen_y = y;
                        sub_x = x; sub_y = y;
                        has_pen = true;
                        break;
                    }
                    case PathVerb::LineTo: {
                        float x = 0.0f, y = 0.0f;
                        if (!reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (has_pen) emit_segment(pen_x, pen_y, x, y);
                        pen_x = x; pen_y = y;
                        has_pen = true;
                        break;
                    }
                    case PathVerb::QuadTo: {
                        float c1x = 0.0f, c1y = 0.0f;
                        float x = 0.0f, y = 0.0f;
                        if (!reader.read_f32(c1x) || !reader.read_f32(c1y)
                            || !reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (has_pen) {
                            flatten_quad(flatten_quad,
                                         pen_x, pen_y, c1x, c1y, x, y,
                                         max_depth);
                        }
                        pen_x = x; pen_y = y;
                        has_pen = true;
                        break;
                    }
                    case PathVerb::CubicTo: {
                        float c1x = 0.0f, c1y = 0.0f;
                        float c2x = 0.0f, c2y = 0.0f;
                        float x = 0.0f, y = 0.0f;
                        if (!reader.read_f32(c1x) || !reader.read_f32(c1y)
                            || !reader.read_f32(c2x) || !reader.read_f32(c2y)
                            || !reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (has_pen) {
                            flatten_cubic(flatten_cubic,
                                          pen_x, pen_y, c1x, c1y,
                                          c2x, c2y, x, y, max_depth);
                        }
                        pen_x = x; pen_y = y;
                        has_pen = true;
                        break;
                    }
                    case PathVerb::ArcTo: {
                        float acx = 0.0f, acy = 0.0f, ar = 0.0f;
                        float asa = 0.0f, aea = 0.0f;
                        if (!reader.read_f32(acx) || !reader.read_f32(acy)
                            || !reader.read_f32(ar)
                            || !reader.read_f32(asa) || !reader.read_f32(aea))
                            return false;
                        if (ar > 0.0f) {
                            ArcInstanceGPU inst{};
                            inst.center_radius_thickness[0] = acx;
                            inst.center_radius_thickness[1] = acy;
                            inst.center_radius_thickness[2] = ar;
                            inst.center_radius_thickness[3] = thickness;
                            inst.angles[0] = asa;
                            inst.angles[1] = aea;
                            inst.color[0] = cr;
                            inst.color[1] = cg;
                            inst.color[2] = cb;
                            inst.color[3] = ca;
                            scratch.batches.back().arcs.push_back(inst);
                        }
                        // Path-spec semantics for the pen after an ArcTo
                        // are out of scope here — cad++ chains ArcTo
                        // segments via explicit MoveTo/LineTo when
                        // continuity matters. Leave the pen position
                        // unchanged; future PRs can refine.
                        break;
                    }
                    case PathVerb::Close: {
                        if (has_pen)
                            emit_segment(pen_x, pen_y, sub_x, sub_y);
                        pen_x = sub_x;
                        pen_y = sub_y;
                        break;
                    }
                    default:
                        return false;
                    }
                }
                break;
            }
            case Cmd::FillPath: {
                // Walk the verb stream into a flat polygon, ear-clip
                // it into triangles, then push 3 raw vertices per
                // triangle into the batch's tri_vertices for the
                // dedicated triangle pipeline. Polygon and triangle
                // scratch buffers live on FrameScratch and are
                // cleared (not destroyed) per FillPath, so a frame
                // with 36 095 HATCHes pays for the buffer's high-
                // water mark once instead of allocating 100k+ vectors.
                //
                // Single closed loop only. Self-intersection / multi-
                // loop / hole semantics are out of scope; cad++ HATCH
                // emits one boundary loop per `Painter::fill_path`
                // call so this matches the use case.
                unsigned int packed = 0;
                unsigned int verb_count = 0;
                if (!reader.read_u32(packed)
                    || !reader.read_u32(verb_count))
                    return false;
                auto color = unpack_color(packed);
                float const cr = color.r / 255.0f;
                float const cg = color.g / 255.0f;
                float const cb = color.b / 255.0f;
                float const ca = color.a / 255.0f;

                auto& polygon = scratch.fill_polygon;
                polygon.clear();
                polygon.reserve(verb_count * 2);
                auto append = [&](float x, float y) {
                    polygon.push_back(x);
                    polygon.push_back(y);
                };

                constexpr float flatness2 = 0.25f;  // (0.5 px)^2
                constexpr int max_depth = 16;
                auto flatten_quad = [&](auto& self,
                                        float p0x, float p0y,
                                        float p1x, float p1y,
                                        float p2x, float p2y,
                                        int depth) -> void {
                    float lx = p2x - p0x, ly = p2y - p0y;
                    float llen2 = lx * lx + ly * ly;
                    bool flat = (depth == 0);
                    if (!flat && llen2 > 1e-6f) {
                        float d = (p1x - p0x) * ly - (p1y - p0y) * lx;
                        if (d * d < flatness2 * llen2) flat = true;
                    }
                    if (flat || llen2 < 1e-6f) {
                        append(p2x, p2y); return;
                    }
                    float a0x = (p0x + p1x) * 0.5f, a0y = (p0y + p1y) * 0.5f;
                    float a1x = (p1x + p2x) * 0.5f, a1y = (p1y + p2y) * 0.5f;
                    float mx  = (a0x + a1x) * 0.5f, my  = (a0y + a1y) * 0.5f;
                    self(self, p0x, p0y, a0x, a0y, mx, my, depth - 1);
                    self(self, mx, my, a1x, a1y, p2x, p2y, depth - 1);
                };
                auto flatten_cubic = [&](auto& self,
                                         float p0x, float p0y,
                                         float p1x, float p1y,
                                         float p2x, float p2y,
                                         float p3x, float p3y,
                                         int depth) -> void {
                    float lx = p3x - p0x, ly = p3y - p0y;
                    float llen2 = lx * lx + ly * ly;
                    bool flat = (depth == 0);
                    if (!flat && llen2 > 1e-6f) {
                        float d1 = (p1x - p0x) * ly - (p1y - p0y) * lx;
                        float d2 = (p2x - p0x) * ly - (p2y - p0y) * lx;
                        if (d1 * d1 < flatness2 * llen2
                            && d2 * d2 < flatness2 * llen2) flat = true;
                    }
                    if (flat || llen2 < 1e-6f) {
                        append(p3x, p3y); return;
                    }
                    float a01x = (p0x + p1x) * 0.5f, a01y = (p0y + p1y) * 0.5f;
                    float a12x = (p1x + p2x) * 0.5f, a12y = (p1y + p2y) * 0.5f;
                    float a23x = (p2x + p3x) * 0.5f, a23y = (p2y + p3y) * 0.5f;
                    float b01x = (a01x + a12x) * 0.5f, b01y = (a01y + a12y) * 0.5f;
                    float b12x = (a12x + a23x) * 0.5f, b12y = (a12y + a23y) * 0.5f;
                    float mx   = (b01x + b12x) * 0.5f, my   = (b01y + b12y) * 0.5f;
                    self(self, p0x, p0y, a01x, a01y, b01x, b01y, mx, my, depth - 1);
                    self(self, mx, my, b12x, b12y, a23x, a23y, p3x, p3y, depth - 1);
                };

                // ArcTo discretiser. Fixed 32 segments — adaptive step
                // by radius could refine later; for HATCH-scale arcs
                // (typically ≤ a few hundred pixels) 32 is smooth.
                auto arc_segments = [&](float cx, float cy, float r,
                                        float sa, float ea) {
                    constexpr int N = 32;
                    float sweep = ea - sa;
                    if (sweep <= 0.0f) sweep += 6.2831853f;
                    if (sweep > 6.2831853f) sweep = 6.2831853f;
                    for (int i = 1; i <= N; ++i) {
                        float t = sa + sweep
                                  * static_cast<float>(i)
                                  / static_cast<float>(N);
                        append(cx + r * std::cos(t),
                               cy + r * std::sin(t));
                    }
                };

                bool started = false;
                for (unsigned int i = 0; i < verb_count; ++i) {
                    unsigned int verb = 0;
                    if (!reader.read_u32(verb)) return false;
                    switch (static_cast<PathVerb>(verb)) {
                    case PathVerb::MoveTo: {
                        float x = 0, y = 0;
                        if (!reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (!started) {
                            append(x, y);
                            started = true;
                        }
                        // else: ignore subsequent MoveTos (single-loop
                        // fill only — multi-loop / hole semantics are
                        // out of scope for now).
                        break;
                    }
                    case PathVerb::LineTo: {
                        float x = 0, y = 0;
                        if (!reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        append(x, y);
                        if (!started) started = true;
                        break;
                    }
                    case PathVerb::QuadTo: {
                        float c1x = 0, c1y = 0, x = 0, y = 0;
                        if (!reader.read_f32(c1x) || !reader.read_f32(c1y)
                            || !reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (!polygon.empty()) {
                            float p0x = polygon[polygon.size() - 2];
                            float p0y = polygon[polygon.size() - 1];
                            flatten_quad(flatten_quad, p0x, p0y,
                                         c1x, c1y, x, y, max_depth);
                        }
                        break;
                    }
                    case PathVerb::CubicTo: {
                        float c1x = 0, c1y = 0, c2x = 0, c2y = 0;
                        float x = 0, y = 0;
                        if (!reader.read_f32(c1x) || !reader.read_f32(c1y)
                            || !reader.read_f32(c2x) || !reader.read_f32(c2y)
                            || !reader.read_f32(x) || !reader.read_f32(y))
                            return false;
                        if (!polygon.empty()) {
                            float p0x = polygon[polygon.size() - 2];
                            float p0y = polygon[polygon.size() - 1];
                            flatten_cubic(flatten_cubic, p0x, p0y,
                                          c1x, c1y, c2x, c2y,
                                          x, y, max_depth);
                        }
                        break;
                    }
                    case PathVerb::ArcTo: {
                        float acx = 0, acy = 0, ar = 0;
                        float asa = 0, aea = 0;
                        if (!reader.read_f32(acx) || !reader.read_f32(acy)
                            || !reader.read_f32(ar)
                            || !reader.read_f32(asa) || !reader.read_f32(aea))
                            return false;
                        if (ar > 0.0f) arc_segments(acx, acy, ar, asa, aea);
                        break;
                    }
                    case PathVerb::Close:
                        // Close is implicit on a fill — the polygon
                        // wraps to its first vertex automatically.
                        break;
                    default:
                        return false;
                    }
                }

                if (polygon.size() < 6) break;  // < 3 vertices
                auto& tris = scratch.fill_tris;
                tris.clear();
                polygon_ear_clip(polygon, tris, scratch.fill_ear_remain);
                // Append vertices via push_back so the vector's own
                // amortised doubling growth applies. A naive
                // dst.reserve(dst.size() + small_delta) here is a
                // performance trap: libc++'s reserve allocates
                // EXACTLY the requested capacity, so calling it once
                // per HATCH with a small delta forces a realloc-and-
                // copy on every fill — O(N²) cumulative on dense
                // CAD content (36 095 fills × growing buffer ≈ 2 s).
                ensure_triangle_order_batch(scratch);
                auto& dst = scratch.batches.back().tri_vertices;
                for (std::size_t t = 0; t + 5 < tris.size(); t += 6) {
                    // Push three vertices for this triangle. Hardware
                    // rasterisation handles edge coverage exactly so
                    // adjacent triangles tile pixel-perfectly without
                    // the sub-pixel gaps the previous CPU scanline
                    // path produced for slim hatch slivers.
                    TriVertexGPU v;
                    v.color[0] = cr; v.color[1] = cg;
                    v.color[2] = cb; v.color[3] = ca;
                    v.pos[0] = tris[t];     v.pos[1] = tris[t + 1]; dst.push_back(v);
                    v.pos[0] = tris[t + 2]; v.pos[1] = tris[t + 3]; dst.push_back(v);
                    v.pos[0] = tris[t + 4]; v.pos[1] = tris[t + 5]; dst.push_back(v);
                }
                break;
            }
            case Cmd::FillQuads: {
                unsigned int count = 0;
                if (!reader.read_u32(count))
                    return false;
                ensure_triangle_order_batch(scratch);
                auto& dst = scratch.batches.back().tri_vertices;
                dst.reserve(dst.size() + static_cast<std::size_t>(count) * 6);
                for (unsigned int i = 0; i < count; ++i) {
                    unsigned int packed = 0;
                    float x0 = 0.0f, y0 = 0.0f;
                    float x1 = 0.0f, y1 = 0.0f;
                    float x2 = 0.0f, y2 = 0.0f;
                    float x3 = 0.0f, y3 = 0.0f;
                    if (!reader.read_u32(packed)
                        || !reader.read_f32(x0) || !reader.read_f32(y0)
                        || !reader.read_f32(x1) || !reader.read_f32(y1)
                        || !reader.read_f32(x2) || !reader.read_f32(y2)
                        || !reader.read_f32(x3) || !reader.read_f32(y3))
                        return false;
                    auto color = unpack_color(packed);
                    TriVertexGPU v;
                    v.color[0] = color.r / 255.0f;
                    v.color[1] = color.g / 255.0f;
                    v.color[2] = color.b / 255.0f;
                    v.color[3] = color.a / 255.0f;
                    v.pos[0] = x0; v.pos[1] = y0; dst.push_back(v);
                    v.pos[0] = x1; v.pos[1] = y1; dst.push_back(v);
                    v.pos[0] = x2; v.pos[1] = y2; dst.push_back(v);
                    v.pos[0] = x0; v.pos[1] = y0; dst.push_back(v);
                    v.pos[0] = x2; v.pos[1] = y2; dst.push_back(v);
                    v.pos[0] = x3; v.pos[1] = y3; dst.push_back(v);
                }
                break;
            }
            case Cmd::FillRects: {
                unsigned int count = 0;
                if (!reader.read_u32(count))
                    return false;
                for (unsigned int i = 0; i < count; ++i) {
                    float x = 0.0f, y = 0.0f;
                    float w = 0.0f, h = 0.0f;
                    unsigned int packed = 0;
                    if (!reader.read_f32(x) || !reader.read_f32(y)
                        || !reader.read_f32(w) || !reader.read_f32(h)
                        || !reader.read_u32(packed))
                        return false;
                    auto color = unpack_color(packed);
                    append_color_instance(
                        scratch.batches.back().colors,
                        x, y, w, h,
                        color.r / 255.0f, color.g / 255.0f,
                        color.b / 255.0f, color.a / 255.0f);
                }
                break;
            }
            case Cmd::LinearGradientRect: {
                float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
                unsigned int from_packed = 0;
                unsigned int to_packed = 0;
                unsigned int axis_raw = 0;
                unsigned int steps = 0;
                if (!reader.read_f32(x) || !reader.read_f32(y)
                    || !reader.read_f32(w) || !reader.read_f32(h)
                    || !reader.read_u32(from_packed)
                    || !reader.read_u32(to_packed)
                    || !reader.read_u32(axis_raw)
                    || !reader.read_u32(steps))
                    return false;
                append_linear_gradient_instances(
                    scratch.batches.back().colors,
                    x,
                    y,
                    w,
                    h,
                    unpack_color(from_packed),
                    unpack_color(to_packed),
                    gradient_axis_from_wire(axis_raw),
                    steps);
                break;
            }
            default:
                return false;
        }
    }
    return true;
}

} // namespace phenotype::native::detail
#endif

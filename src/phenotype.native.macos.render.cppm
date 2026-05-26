module;
#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#endif

export module phenotype.native.macos.render;

#if !defined(__wasi__) && !defined(__ANDROID__) && defined(__APPLE__)
import phenotype.material;
import phenotype.native.macos.text;

export namespace phenotype::native::detail {

struct ColorInstanceGPU {
    float rect[4]{};
    float color[4]{};
    float params[4]{};
};

struct MaterialInstanceGPU {
    float rect[4]{};
    float tint[4]{};
    // radius, blur radius, opacity, resolved sample taps
    float params[4]{};
    // saturation, luminance floor, luminance gain, edge highlight
    float optics[4]{};
    // edge width, shadow alpha, shadow radius, noise opacity
    float effects[4]{};
    // blur step scale, kernel radius, reserved, reserved
    float sampling[4]{};
    // gamma, midpoint, contrast, edge lift
    float luminance_curve[4]{};
    // specular anchor x/y, radius, intensity
    float interaction[4]{};
    // pointer lens anchor x/y, radius, strength
    float interaction_lens[4]{};
    // control morph scale delta, depth, edge lift, shadow compression
    float control_morph[4]{};
    // materialize wave strength, edge lift, lensing gain, rim position
    float transition_optics[4]{};
    // refraction strength, edge bias, max offset pixels, edge caustic intensity
    float refraction[4]{};
    // bevel width, inner highlight, outer shadow, chromatic fringe
    float edge_optics[4]{};
    // spectral warmth, coolness, dispersion, rim tint
    float spectral_tint[4]{};
    // dynamic light direction x/y, highlight strength, shadow strength
    float lighting[4]{};
    // glass thickness, lensing gain, shadow gain, scattering gain
    float thickness[4]{};
    // axial offset, tangential offset, prismatic gain, caustic spread
    float dispersion[4]{};
    // fade extent, dissolve, dimming, hard style
    float scroll_edge[4]{};
    // intensity, tint weight, edge lift, lensing gain
    float prominent_glass[4]{};
    // dimming, contrast lift, brightness response, detail response
    float clear_glass[4]{};
    // stabilization strength, damping, shimmer reduction, transmission bias
    float glass_stability[4]{};
    // environment reflection, color pickup, luminance balance, transmission
    float glass_environment[4]{};
    // surface/union response, edge adhesion/continuity, coalescence, luma stability
    float glass_union[4]{};
    // group x/y/w/h for container edge-continuity execution
    float group_rect[4]{};
    // shape blend strength, inner-edge alpha blend strength, flags, command index
    float group_effects[4]{};
    // fusion strength, lensing gain, edge lift, shadow gain
    float fusion_optics[4]{};
    // cohesion strength, pressure, falloff, stabilization
    float container_cohesion[4]{};
    // bridge direction x/y, anchor x/y for container/matched flow
    float bridge_motion[4]{};
    // bridge strength, flow offset gain, ribbon width, highlight gain
    float bridge_optics[4]{};
};

// Per-vertex GPU layout for the triangle pipeline (FillPath fast path).
// Each ear-clipped triangle pushes 3 of these into the batch's triangle
// vertex buffer; the GPU rasterises with hardware coverage rules so
// adjacent triangles tile pixel-perfectly without sub-pixel gaps. 24
// bytes/vertex × 3 vertices/triangle = 72 bytes/triangle, vs. ~50
// scanlines × 48 bytes/ColorInstance = ~2.4 KB/triangle under the old
// CPU scanline path. A 36k-hatch DWG (colorwh.dwg) drops from millions
// of fill_rect instances to ~70k vertices in one drawPrimitives call.
struct TriVertexGPU {
    float pos[2]{};
    float color[4]{};
};

// Per-arc instance for the SDF arc pipeline. Layout matches the
// shader's `ArcInstance` struct exactly (3 × float4 = 48 bytes).
//   center_radius_thickness = (cx, cy, radius, thickness)
//   angles                  = (start_angle, end_angle, _, _)
//   color                   = (r, g, b, a) linear, 0..1
struct ArcInstanceGPU {
    float center_radius_thickness[4]{};
    float angles[4]{};
    float color[4]{};
};

struct TextInstanceGPU {
    float rect[4]{};
    float uv_rect[4]{};
    float color[4]{};
    // Per-instance rigid-body rotation: `(pivot_x, pivot_y, cos, sin)`.
    // The shader rotates each glyph quad's pre-rotation pixel position
    // around `(pivot_x, pivot_y)` by the angle whose cosine and sine
    // are stored here. Default `(0,0,1,0)` is identity — packs the
    // axis-aligned glyph instance bit-for-bit the way it always did.
    float rot[4]{0.0f, 0.0f, 1.0f, 0.0f};
};

struct ParsedTextRun {
    float x = 0.0f;
    float y = 0.0f;
    float font_size = 0.0f;
    // Radians, CCW about pivot `(x, y)`. Default 0.0f reproduces
    // the unrotated glyph atlas path; nonzero passes through to the
    // text vertex shader via TextInstanceGPU::rot.
    float rotation = 0.0f;
    // Horizontal glyph stretch — see `FontSpec::width_factor`. We
    // multiply each glyph's pixel-space horizontal advance and width
    // by this when prepare_text_instances lays the run out, so the
    // glyphs end up rendered wider/narrower in place along the run's
    // local X axis. Default 1.0f keeps the atlas-native advance.
    float width_factor = 1.0f;
    bool mono = false;
    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 1.0f;
    float line_height = 0.0f;
    char const* text = nullptr;
    unsigned int len = 0;
    // Index into FrameScratch::batches that owns this run, or UINT32_MAX
    // for overlay runs (IME composition / generic caret) that always
    // render full-viewport above scissored scene content.
    std::uint32_t batch_idx = 0;
    // Resolved FontCacheKey (family / weight / style / mono); driven
    // by the wire-format flags + family bytes at decode time.
    FontCacheKey font_key{};
};

struct RasterizedTextRun {
    std::vector<std::uint8_t> alpha;
    float x_offset = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    int pixel_width = 0;
    int pixel_height = 0;
    bool has_ink = false;
};

struct TextCacheEntry {
    std::string text;
    int font_size_key = 0;
    int line_height_key = 0;
    int scale_key = 0;
    // Quantised FontSpec.width_factor — different stretches need
    // different rasterised glyphs (the matrix bakes the X scale into
    // the atlas alpha pixels), so a single (text + font + size)
    // tuple can occupy multiple atlas entries when the same caller
    // draws the same string at different stretches.
    int width_factor_key = 0;
    bool mono = false;
    float x_offset = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
    float u = 0.0f;
    float v = 0.0f;
    float uw = 0.0f;
    float vh = 0.0f;
    bool has_ink = false;
    FontCacheKey font_key{};
};

struct TextAtlasCache {
    static constexpr int atlas_size = 4096;

    std::vector<TextCacheEntry> entries;
    std::vector<std::uint8_t> pixels;
    int cursor_x = 0;
    int cursor_y = 0;
    int row_height = 0;
    bool dirty = false;
    int dirty_min_x = atlas_size;
    int dirty_min_y = atlas_size;
    int dirty_max_x = 0;
    int dirty_max_y = 0;
    int active_scale_key = 0;
};

struct PendingImageCmd {
    float x = 0.0f;
    float y = 0.0f;
    float w = 0.0f;
    float h = 0.0f;
    std::string url;
    // Index into FrameScratch::batches that owns this image. The lazy
    // image-cache resolution in prepare_image_instances pushes the
    // resolved instance (or placeholder colors) into the matching
    // batch's per-pipeline vector so scissor clipping survives the
    // decode → resolve split.
    std::uint32_t batch_idx = 0;
};

struct ImageInstanceGPU {
    float rect[4]{};
    float uv_rect[4]{};
    float color[4]{};
};

// Cmd::Scissor splits each frame into ordered batches. Within a batch
// instances are pushed in decode order; render time issues one
// vkCmdSetScissor / setScissorRect: per batch and per-pipeline draws
// using firstInstance / baseInstance offsets from the flat staging
// vectors that finalize_batches assembles.
//
// Sentinel rect (x=y=w=h=0.0) means "full drawable" — paint emits this
// to reset clipping (emit_scissor_reset). The first batch of every
// frame is initialized to this sentinel so any draw before a Scissor
// command renders unclipped.
struct ScissorBatch {
    float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f;
    // tri_vertices holds raw triangle-list vertices (3 per triangle)
    // accumulated from FillPath / FillQuads output. Drawn before colors
    // in the same batch; the decoder opens a same-scissor batch when a
    // later triangle primitive must appear above prior color instances.
    std::vector<TriVertexGPU>     tri_vertices;
    std::vector<ColorInstanceGPU> colors;
    std::vector<MaterialInstanceGPU> materials;
    std::vector<ArcInstanceGPU>   arcs;
    std::vector<ImageInstanceGPU> images;
    std::vector<TextInstanceGPU>  texts;
    // DrawText commands are decoded into text_runs first and materialized into
    // texts after atlas preparation. Track them here so text-only scissored
    // canvases still keep their own batch instead of being treated as empty.
    bool pending_text_runs = false;
    std::uint32_t tri_first   = 0;
    std::uint32_t color_first = 0;
    std::uint32_t material_first = 0;
    std::uint32_t arc_first   = 0;
    std::uint32_t image_first = 0;
    std::uint32_t text_first  = 0;
};

struct MaterialPaintLayerRange {
    std::uint32_t command_index = 0;
    std::size_t batch_index = 0;
    std::size_t first = 0;
    std::size_t count = 0;
};

struct FrameScratch {
    // Per-batch local accumulators. Concatenated into the flat
    // *_instances vectors below by finalize_batches().
    std::vector<ScissorBatch> batches;

    // Flat staging vectors uploaded to GPU instance buffers. Populated
    // by finalize_batches() — direct decode/prepare pushes go to the
    // per-batch locals above.
    std::vector<TriVertexGPU>     tri_vertices;
    std::vector<ColorInstanceGPU> color_instances;
    std::vector<MaterialInstanceGPU> material_instances;
    std::vector<ArcInstanceGPU>   arc_instances;
    std::vector<ImageInstanceGPU> image_instances;
    std::vector<TextInstanceGPU> text_instances;

    // Overlays (IME caret/underline + composition text) are Z-above
    // scene content and always render full-viewport, outside the
    // batch loop.
    std::vector<ColorInstanceGPU> overlay_color_instances;
    std::vector<TextInstanceGPU> overlay_text_instances;

    std::vector<ParsedTextRun> text_runs;
    std::vector<PendingImageCmd> images;
    std::vector<std::string> overlay_text_storage;
    std::vector<MaterialRuntimeRecord> material_records;
    std::vector<MaterialContainerExecutionDescriptor>
        material_container_execution_descriptors;
    std::vector<MaterialPaintLayerRange> material_paint_layer_ranges;
    std::uint32_t foreground_text_candidate_count = 0;
    std::uint32_t foreground_text_remap_count = 0;
    std::uint32_t full_frame_opaque_fill_count = 0;

    // Per-FillPath scratch buffers, reused across every Cmd::FillPath
    // decode in a single frame. CAD HATCH-heavy content (e.g.
    // colorwh.dwg with 36 095 fills) used to default-construct these
    // three std::vectors inside the case block, costing ~100k heap
    // allocations per frame. Hoisting them out keeps allocator
    // pressure flat as the canvas count scales.
    std::vector<float>       fill_polygon;
    std::vector<float>       fill_tris;
    std::vector<std::size_t> fill_ear_remain;

    // Slice within `text_instances` that contains overlay text. Set by
    // finalize_batches() — overlay texts are appended after all batched
    // scene texts so a single instance buffer holds both. Drawn after
    // the batch loop with full-viewport scissor.
    std::uint32_t overlay_text_first = 0;
    std::uint32_t overlay_text_count = 0;

    void clear() {
        batches.clear();
        batches.push_back(ScissorBatch{});  // sentinel: full viewport
        tri_vertices.clear();
        color_instances.clear();
        material_instances.clear();
        arc_instances.clear();
        overlay_color_instances.clear();
        text_runs.clear();
        images.clear();
        image_instances.clear();
        text_instances.clear();
        overlay_text_instances.clear();
        overlay_text_storage.clear();
        material_records.clear();
        material_container_execution_descriptors.clear();
        material_paint_layer_ranges.clear();
        foreground_text_candidate_count = 0;
        foreground_text_remap_count = 0;
        full_frame_opaque_fill_count = 0;
        overlay_text_first = 0;
        overlay_text_count = 0;
    }
};

inline void open_scissor_batch(FrameScratch& s,
                               float x, float y, float w, float h) {
    auto& cur = s.batches.back();
    if (cur.tri_vertices.empty() && cur.colors.empty()
        && cur.materials.empty() && cur.arcs.empty()
        && cur.images.empty() && cur.texts.empty()
        && !cur.pending_text_runs) {
        cur.x = x; cur.y = y; cur.w = w; cur.h = h;
        return;
    }
    ScissorBatch next;
    next.x = x; next.y = y; next.w = w; next.h = h;
    s.batches.push_back(std::move(next));
}

inline void open_same_scissor_batch(FrameScratch& s) {
    auto const& cur = s.batches.back();
    open_scissor_batch(s, cur.x, cur.y, cur.w, cur.h);
}

inline void ensure_triangle_order_batch(FrameScratch& s) {
    auto const& cur = s.batches.back();
    if (!cur.colors.empty() || !cur.materials.empty() || !cur.arcs.empty()
        || !cur.images.empty() || !cur.texts.empty()
        || cur.pending_text_runs) {
        open_same_scissor_batch(s);
    }
}

inline void finalize_batches(FrameScratch& s) {
    s.tri_vertices.clear();
    s.color_instances.clear();
    s.material_instances.clear();
    s.arc_instances.clear();
    s.image_instances.clear();
    s.text_instances.clear();
    for (auto& b : s.batches) {
        b.tri_first   = static_cast<std::uint32_t>(s.tri_vertices.size());
        b.color_first = static_cast<std::uint32_t>(s.color_instances.size());
        b.material_first = static_cast<std::uint32_t>(s.material_instances.size());
        b.arc_first   = static_cast<std::uint32_t>(s.arc_instances.size());
        b.image_first = static_cast<std::uint32_t>(s.image_instances.size());
        b.text_first  = static_cast<std::uint32_t>(s.text_instances.size());
        s.tri_vertices.insert(s.tri_vertices.end(),
                              b.tri_vertices.begin(), b.tri_vertices.end());
        s.color_instances.insert(s.color_instances.end(),
                                 b.colors.begin(), b.colors.end());
        s.material_instances.insert(s.material_instances.end(),
                                    b.materials.begin(), b.materials.end());
        s.arc_instances.insert(s.arc_instances.end(),
                               b.arcs.begin(), b.arcs.end());
        s.image_instances.insert(s.image_instances.end(),
                                 b.images.begin(), b.images.end());
        s.text_instances.insert(s.text_instances.end(),
                                b.texts.begin(), b.texts.end());
    }
    s.overlay_text_first =
        static_cast<std::uint32_t>(s.text_instances.size());
    s.overlay_text_count =
        static_cast<std::uint32_t>(s.overlay_text_instances.size());
    s.text_instances.insert(s.text_instances.end(),
                            s.overlay_text_instances.begin(),
                            s.overlay_text_instances.end());
}

} // namespace phenotype::native::detail
#endif

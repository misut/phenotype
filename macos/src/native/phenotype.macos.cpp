#import <AppKit/AppKit.h>
#import <CoreText/CoreText.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
#include <webgpu/webgpu_cpp.h>

#include <phenotype/layout.hpp>
#include <phenotype/macos.hpp>
#include <phenotype/scene.hpp>

// The scene contract (LayoutRect/SceneLayout/...) lives in phenotype/scene.hpp
// and the layout engine that fills it in phenotype/layout.hpp. Pull both into
// scope so the renderer and the AppKit delegate (outside the anonymous
// namespace) share the same types.
using namespace phenotype::scene;
using namespace phenotype::layout;

namespace {

constexpr uint32_t kTextAtlasPadding = 2;
constexpr uint32_t kTextAtlasRowAlignmentPixels = 64;

// The glyph atlas packs at most this many text / symbol entries per frame. The
// scene record vectors are now unbounded (storage buffers), but the atlas is a
// fixed-size texture, so glyph entries beyond these bounds are not rasterized.
// These are renderer-local and independent of the scene reserve hints.
constexpr size_t kTextAtlasEntryLimit = 128;
constexpr size_t kSymbolAtlasEntryLimit = 128;

constexpr char kButtonShader[] = R"wgsl(
struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) pixel_position : vec2f,
    // instance_index is vertex-only; carry it to the fragment stage as a flat
    // varying so each kind's fragment knows which record it is shading.
    @location(1) @interpolate(flat) instance : u32,
};

struct SymbolButton {
    frame : vec4f,
    icon : vec4f,
    style : vec4f,
    control : vec4f,
    appearance : vec4f,
    color : vec4f,
    clip : vec4f,
};

struct Panel {
    frame : vec4f,
    color : vec4f,
    style : vec4f,
    // shadow_color.rgb is the tint, .a the peak opacity. shadow_params is
    // (offset_x, offset_y, blur_radius, _) in pixels. A zero alpha disables it.
    shadow_color : vec4f,
    shadow_params : vec4f,
    clip : vec4f,
};

struct TextRun {
    frame : vec4f,
    uv : vec4f,
    color : vec4f,
    clip : vec4f,
};

struct SceneHeader {
    viewport : vec4f,
    counts : vec4f,
};

// Per-kind draw records live in storage buffers, so the scene is bounded only
// by the storage-buffer size limit (128 MiB) instead of the old fixed
// 16/128/128 uniform-array caps. The fragment compositing is unchanged.
@group(0) @binding(0) var<uniform> scene : SceneHeader;
@group(0) @binding(1) var<storage, read> panels : array<Panel>;
@group(0) @binding(2) var<storage, read> buttons : array<SymbolButton>;
@group(0) @binding(3) var<storage, read> texts : array<TextRun>;
@group(0) @binding(4) var text_atlas : texture_2d<f32>;
@group(0) @binding(5) var text_sampler : sampler;

// One instance is drawn as a 6-vertex quad covering a pixel-space rectangle
// (min..max). The rect is the instance's bounds padded a couple of pixels so
// the SDF anti-aliasing fringe (which extends ~1px past the geometry edge) is
// not clipped — coverage outside the rect is zero, so padding is free.
fn quadVertex(index : u32, instance : u32, min_px : vec2f, max_px : vec2f) -> VertexOut {
    let corners = array<vec2f, 6>(
        vec2f(0.0, 0.0),
        vec2f(0.0, 1.0),
        vec2f(1.0, 0.0),
        vec2f(1.0, 0.0),
        vec2f(0.0, 1.0),
        vec2f(1.0, 1.0),
    );
    let corner = corners[index];
    let pixel_position = mix(min_px, max_px, corner);

    let viewport_size = max(scene.viewport.xy, vec2f(1.0));
    let ndc = vec2f(
        (pixel_position.x / viewport_size.x) * 2.0 - 1.0,
        1.0 - (pixel_position.y / viewport_size.y) * 2.0,
    );

    var out : VertexOut;
    out.position = vec4f(ndc, 0.0, 1.0);
    out.pixel_position = pixel_position;
    out.instance = instance;
    return out;
}

// Expand a pixel rect by the AA fringe and intersect with the clip rect, so the
// quad never covers more than the clipped region (matches clipCoverage).
fn paddedClippedRect(min_px : vec2f, max_px : vec2f, clip : vec4f) -> array<vec2f, 2> {
    let pad = vec2f(2.0 * max(scene.viewport.w, 1.0));
    let lo = max(min_px - pad, clip.xy);
    let hi = min(max_px + pad, clip.xy + clip.zw);
    return array<vec2f, 2>(lo, hi);
}

// The drop shadow cast by a button's control fill, matching the surface card's
// soft look at a smaller scale. Shared by buttonVertex (to size the quad) and
// drawSymbolButton (to paint it) so the two never disagree on the extent.
fn controlShadowOffset(ui_scale : f32) -> vec2f { return vec2f(0.0, 1.5 * ui_scale); }
fn controlShadowBlur(ui_scale : f32) -> f32 { return 4.0 * ui_scale; }
const kControlShadowAlpha : f32 = 0.18;

@vertex
fn panelVertex(@builtin(vertex_index) vi : u32, @builtin(instance_index) ii : u32) -> VertexOut {
    let panel = panels[ii];
    let half = panel.frame.zw * 0.5;
    let panel_lo = panel.frame.xy - half;
    let panel_hi = panel.frame.xy + half;
    // When a shadow is present, grow the quad to cover its offset+blur extent so
    // the soft falloff outside the panel is not clipped away. Costs nothing when
    // the shadow is disabled (offset and blur are then zero).
    let shadow_lo = panel_lo + panel.shadow_params.xy - vec2f(panel.shadow_params.z);
    let shadow_hi = panel_hi + panel.shadow_params.xy + vec2f(panel.shadow_params.z);
    let bounds = paddedClippedRect(min(panel_lo, shadow_lo), max(panel_hi, shadow_hi), panel.clip);
    return quadVertex(vi, ii, bounds[0], bounds[1]);
}

@vertex
fn buttonVertex(@builtin(vertex_index) vi : u32, @builtin(instance_index) ii : u32) -> VertexOut {
    let button = buttons[ii];
    // Cover the union of the icon frame and the (possibly larger) control box,
    // grown by the control shadow's offset+blur reach so its soft falloff is not
    // clipped away. The reach must match the shadow drawn in drawSymbolButton.
    let ui_scale = max(scene.viewport.w, 1.0);
    let shadow_reach = controlShadowOffset(ui_scale) + vec2f(controlShadowBlur(ui_scale));
    let frame_lo = button.frame.xy - button.frame.zw * 0.5;
    let frame_hi = button.frame.xy + button.frame.zw * 0.5;
    let control_lo = button.control.xy - button.control.zw * 0.5 - shadow_reach;
    let control_hi = button.control.xy + button.control.zw * 0.5 + shadow_reach;
    let bounds = paddedClippedRect(min(frame_lo, control_lo), max(frame_hi, control_hi), button.clip);
    return quadVertex(vi, ii, bounds[0], bounds[1]);
}

@vertex
fn textVertex(@builtin(vertex_index) vi : u32, @builtin(instance_index) ii : u32) -> VertexOut {
    let text = texts[ii];
    let top_left = text.frame.xy - text.frame.zw * 0.5;
    let bounds = paddedClippedRect(top_left, top_left + text.frame.zw, text.clip);
    return quadVertex(vi, ii, bounds[0], bounds[1]);
}

fn roundedRectDistance(position : vec2f, half_size : vec2f, radius : f32) -> f32 {
    let q = abs(position) - (half_size - vec2f(radius));
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

fn rectDistance(position : vec2f, half_size : vec2f) -> f32 {
    let q = abs(position) - half_size;
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0);
}

fn topRoundedRectDistance(position : vec2f, half_size : vec2f, radius : f32) -> f32 {
    let x = abs(position.x);
    let y_from_top = position.y + half_size.y;
    if (y_from_top < radius && x > half_size.x - radius) {
        let corner_center = vec2f(half_size.x - radius, radius);
        return length(vec2f(x, y_from_top) - corner_center) - radius;
    }
    return rectDistance(position, half_size);
}

fn bottomRoundedRectDistance(position : vec2f, half_size : vec2f, radius : f32) -> f32 {
    let x = abs(position.x);
    let y_from_bottom = half_size.y - position.y;
    if (y_from_bottom < radius && x > half_size.x - radius) {
        let corner_center = vec2f(half_size.x - radius, radius);
        return length(vec2f(x, y_from_bottom) - corner_center) - radius;
    }
    return rectDistance(position, half_size);
}

fn panelDistance(position : vec2f, half_size : vec2f, radius : f32, corner_mode : f32) -> f32 {
    if (radius <= 0.0) {
        return rectDistance(position, half_size);
    }
    if (corner_mode > 1.5) {
        return bottomRoundedRectDistance(position, half_size, radius);
    }
    if (corner_mode > 0.5) {
        return topRoundedRectDistance(position, half_size, radius);
    }
    return roundedRectDistance(position, half_size, radius);
}

fn compositeOver(destination : vec4f, source_color : vec3f, source_alpha : f32) -> vec4f {
    let alpha = clamp(source_alpha, 0.0, 1.0);
    return vec4f(
        destination.rgb * (1.0 - alpha) + source_color * alpha,
        destination.a * (1.0 - alpha) + alpha,
    );
}

fn controlRadius(size : vec2f, shape : f32, ui_scale : f32) -> f32 {
    if (shape > 0.5) {
        return min(size.x, size.y) * 0.5;
    }
    return min(10.0 * ui_scale, min(size.x, size.y) * 0.5);
}

fn clipCoverage(pixel_position : vec2f, clip : vec4f) -> f32 {
    let local = pixel_position - clip.xy;
    return step(0.0, local.x) *
        step(0.0, local.y) *
        step(local.x, clip.z) *
        step(local.y, clip.w);
}

fn drawSymbolButton(layer : vec4f, pixel_position : vec2f, button : SymbolButton) -> vec4f {
    let ui_scale = max(scene.viewport.w, 1.0);
    let clip_coverage = clipCoverage(pixel_position, button.clip);
    let control_center = button.control.xy;
    let control_size = button.control.zw;
    let radius = controlRadius(control_size, button.appearance.x, ui_scale);
    let local_control_position = pixel_position - control_center;

    let control_edge = roundedRectDistance(local_control_position, control_size * 0.5, radius);
    let control_coverage = 1.0 - smoothstep(-1.0, 1.0, control_edge);
    let button_fill = vec3f(0.985, 0.988, 0.992);
    let button_border = vec3f(0.73, 0.76, 0.82);

    var out_layer = layer;
    if (button.appearance.y > 0.5) {
        // Soft drop shadow in place of the old hairline border: paint it under
        // the fill and only outside the control shape, so it lifts the control
        // off a solid background the same way the surface card's shadow does.
        let blur = controlShadowBlur(ui_scale);
        let shadow_control_position = local_control_position - controlShadowOffset(ui_scale);
        let shadow_edge = roundedRectDistance(shadow_control_position, control_size * 0.5, radius);
        let shadow_shape = 1.0 - smoothstep(-blur, blur, shadow_edge);
        out_layer = compositeOver(out_layer, vec3f(0.0),
            kControlShadowAlpha * shadow_shape * (1.0 - control_coverage) * clip_coverage);

        out_layer = compositeOver(out_layer, button_fill, control_coverage * clip_coverage * 0.84);

        if (button.appearance.w > 0.5) {
            let divider_distance = abs(pixel_position.x - button.appearance.z) - (0.5 * ui_scale);
            let divider_coverage = 1.0 - smoothstep(-1.0, 1.0, divider_distance);
            let divider_height = max(0.0, control_size.y - (16.0 * ui_scale));
            let divider_y_distance = abs(local_control_position.y) - (divider_height * 0.5);
            let divider_y_coverage = 1.0 - smoothstep(-1.0, 1.0, divider_y_distance);
            out_layer = compositeOver(
                out_layer,
                button_border,
                divider_coverage * divider_y_coverage * control_coverage * clip_coverage * 0.45,
            );
        }
    }

    let top_left = button.frame.xy - (button.frame.zw * 0.5);
    let local_position = pixel_position - top_left;
    let inside = step(0.0, local_position.x) *
        step(0.0, local_position.y) *
        step(local_position.x, button.frame.z) *
        step(local_position.y, button.frame.w);
    let local_uv = local_position / max(button.frame.zw, vec2f(1.0));
    let uv = button.icon.xy + ((button.icon.zw - button.icon.xy) * local_uv);
    let sample_alpha = textureSample(text_atlas, text_sampler, uv).a;
    return compositeOver(
        out_layer,
        button.color.rgb,
        button.color.a * sample_alpha * inside * control_coverage * clip_coverage,
    );
}

fn drawPanel(layer : vec4f, pixel_position : vec2f, panel : Panel) -> vec4f {
    let ui_scale = max(scene.viewport.w, 1.0);
    let clip_coverage = clipCoverage(pixel_position, panel.clip);
    let local_position = pixel_position - panel.frame.xy;
    let half_size = panel.frame.zw * 0.5;
    let radius = min(panel.style.x * ui_scale, min(half_size.x, half_size.y));
    let edge_distance = panelDistance(local_position, half_size, radius, panel.style.y);
    let coverage = 1.0 - smoothstep(-1.0, 1.0, edge_distance);

    var out_layer = layer;

    // Soft drop shadow, painted under the panel and only where the panel itself
    // does not cover, so a translucent fill never darkens its own interior. The
    // blurred shape is approximated by smoothstepping the offset panel's SDF
    // across the blur radius — cheap, and visually a close match to a gaussian
    // for the small radii used by surface shadows.
    let blur = panel.shadow_params.z;
    if (panel.shadow_color.a > 0.0 && blur > 0.0) {
        let shadow_local = local_position - panel.shadow_params.xy;
        let shadow_distance = panelDistance(shadow_local, half_size, radius, panel.style.y);
        let shadow_shape = 1.0 - smoothstep(-blur, blur, shadow_distance);
        let shadow_alpha =
            panel.shadow_color.a * shadow_shape * (1.0 - coverage) * clip_coverage;
        out_layer = compositeOver(out_layer, panel.shadow_color.rgb, shadow_alpha);
    }

    return compositeOver(out_layer, panel.color.rgb, panel.color.a * coverage * clip_coverage);
}

fn drawText(layer : vec4f, pixel_position : vec2f, text : TextRun) -> vec4f {
    let clip_coverage = clipCoverage(pixel_position, text.clip);
    let top_left = text.frame.xy - (text.frame.zw * 0.5);
    let local_position = pixel_position - top_left;
    let inside = step(0.0, local_position.x) *
        step(0.0, local_position.y) *
        step(local_position.x, text.frame.z) *
        step(local_position.y, text.frame.w);
    let local_uv = local_position / max(text.frame.zw, vec2f(1.0));
    let uv = text.uv.xy + ((text.uv.zw - text.uv.xy) * local_uv);
    let sample_alpha = textureSample(text_atlas, text_sampler, uv).a;
    return compositeOver(layer, text.color.rgb, text.color.a * sample_alpha * inside * clip_coverage);
}

// Each kind is drawn as one instanced pass. A draw function returns the
// premultiplied "source over transparent" color for this record; the pipeline's
// premultiplied (One / OneMinusSrcAlpha) blend then composites it onto the
// target. Premultiplied over is associative, and instances rasterize in array
// order, so drawing panels then buttons then texts is pixel-identical to the
// old single-pass loop — only now each fragment touches one record over its own
// quad instead of every record over the whole screen.
@fragment
fn panelFragment(in : VertexOut) -> @location(0) vec4f {
    return drawPanel(vec4f(0.0), in.pixel_position, panels[in.instance]);
}

@fragment
fn buttonFragment(in : VertexOut) -> @location(0) vec4f {
    return drawSymbolButton(vec4f(0.0), in.pixel_position, buttons[in.instance]);
}

@fragment
fn textFragment(in : VertexOut) -> @location(0) vec4f {
    return drawText(vec4f(0.0), in.pixel_position, texts[in.instance]);
}
)wgsl";

constexpr char kEffectShader[] = R"wgsl(
struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) pixel_position : vec2f,
};

struct EffectPanel {
    frame : vec4f,
    color : vec4f,
    style : vec4f,
    clip : vec4f,
};

struct EffectUniforms {
    viewport : vec4f,
    counts : vec4f,
    effects : array<EffectPanel, 8>,
};

@group(0) @binding(0) var<uniform> effect_scene : EffectUniforms;
@group(0) @binding(1) var scene_texture : texture_2d<f32>;
@group(0) @binding(2) var blurred_texture : texture_2d<f32>;
@group(0) @binding(3) var source_sampler : sampler;

@vertex
fn vertexMain(@builtin(vertex_index) index : u32) -> VertexOut {
    let clip = array<vec2f, 6>(
        vec2f(-1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0, -1.0),
        vec2f( 1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0,  1.0),
    );

    let viewport_size = effect_scene.viewport.xy;
    let pixel_position = vec2f(
        (clip[index].x + 1.0) * 0.5 * viewport_size.x,
        (1.0 - clip[index].y) * 0.5 * viewport_size.y,
    );

    var out : VertexOut;
    out.position = vec4f(clip[index], 0.0, 1.0);
    out.pixel_position = pixel_position;
    return out;
}

fn roundedRectDistance(position : vec2f, half_size : vec2f, radius : f32) -> f32 {
    let q = abs(position) - (half_size - vec2f(radius));
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0) - radius;
}

fn rectDistance(position : vec2f, half_size : vec2f) -> f32 {
    let q = abs(position) - half_size;
    return length(max(q, vec2f(0.0))) + min(max(q.x, q.y), 0.0);
}

fn topRoundedRectDistance(position : vec2f, half_size : vec2f, radius : f32) -> f32 {
    let x = abs(position.x);
    let y_from_top = position.y + half_size.y;
    if (y_from_top < radius && x > half_size.x - radius) {
        let corner_center = vec2f(half_size.x - radius, radius);
        return length(vec2f(x, y_from_top) - corner_center) - radius;
    }
    return rectDistance(position, half_size);
}

fn bottomRoundedRectDistance(position : vec2f, half_size : vec2f, radius : f32) -> f32 {
    let x = abs(position.x);
    let y_from_bottom = half_size.y - position.y;
    if (y_from_bottom < radius && x > half_size.x - radius) {
        let corner_center = vec2f(half_size.x - radius, radius);
        return length(vec2f(x, y_from_bottom) - corner_center) - radius;
    }
    return rectDistance(position, half_size);
}

fn panelDistance(position : vec2f, half_size : vec2f, radius : f32, corner_mode : f32) -> f32 {
    if (radius <= 0.0) {
        return rectDistance(position, half_size);
    }
    if (corner_mode > 1.5) {
        return bottomRoundedRectDistance(position, half_size, radius);
    }
    if (corner_mode > 0.5) {
        return topRoundedRectDistance(position, half_size, radius);
    }
    return roundedRectDistance(position, half_size, radius);
}

fn clipCoverage(pixel_position : vec2f, clip : vec4f) -> f32 {
    let local = pixel_position - clip.xy;
    return step(0.0, local.x) *
        step(0.0, local.y) *
        step(local.x, clip.z) *
        step(local.y, clip.w);
}

fn compositeOver(destination : vec4f, source_color : vec3f, source_alpha : f32) -> vec4f {
    let alpha = clamp(source_alpha, 0.0, 1.0);
    return vec4f(
        destination.rgb * (1.0 - alpha) + source_color * alpha,
        destination.a * (1.0 - alpha) + alpha,
    );
}

fn sampleScene(pixel_position : vec2f) -> vec4f {
    let uv = clamp(pixel_position / max(effect_scene.viewport.xy, vec2f(1.0)), vec2f(0.0), vec2f(1.0));
    return textureSampleLevel(scene_texture, source_sampler, uv, 0.0);
}

fn sampleBlurred(pixel_position : vec2f) -> vec4f {
    let uv = clamp(pixel_position / max(effect_scene.viewport.xy, vec2f(1.0)), vec2f(0.0), vec2f(1.0));
    return textureSampleLevel(blurred_texture, source_sampler, uv, 0.0);
}

fn tintPremultiplied(layer : vec4f, tint_color : vec3f, tint_strength : f32) -> vec4f {
    let alpha = clamp(layer.a, 0.0, 1.0);
    let tint = tint_color * alpha;
    return vec4f(mix(layer.rgb, tint, clamp(tint_strength, 0.0, 1.0)), alpha);
}

fn effectCoverage(pixel_position : vec2f, panel : EffectPanel) -> f32 {
    let ui_scale = max(effect_scene.viewport.w, 1.0);
    let local_position = pixel_position - panel.frame.xy;
    let half_size = panel.frame.zw * 0.5;
    let radius = min(panel.style.x * ui_scale, min(half_size.x, half_size.y));
    let edge_distance = panelDistance(local_position, half_size, radius, panel.style.y);
    let shape_coverage = 1.0 - smoothstep(-1.0, 1.0, edge_distance);
    let bottom = panel.frame.y + half_size.y;
    let fade_height = 12.0 * ui_scale;
    let bottom_fade = 1.0 - smoothstep(bottom - fade_height, bottom, pixel_position.y);
    return shape_coverage * bottom_fade * clipCoverage(pixel_position, panel.clip);
}

@fragment
fn fragmentMain(in : VertexOut) -> @location(0) vec4f {
    let base_layer = sampleScene(in.pixel_position);
    let backdrop_presence = smoothstep(0.02, 0.20, base_layer.a);
    var layer = base_layer;
    let effect_count = min(u32(effect_scene.counts.x), 8u);
    for (var index = 0u; index < effect_count; index = index + 1u) {
        let panel = effect_scene.effects[index];
        let coverage = effectCoverage(in.pixel_position, panel) * backdrop_presence;
        if (coverage > 0.0) {
            // style.z is the material's blur amount: lerp the sharp backdrop
            // toward its blurred copy so thinner materials stay clearer.
            let blur_amount = clamp(panel.style.z, 0.0, 1.0);
            let backdrop = mix(sampleScene(in.pixel_position), sampleBlurred(in.pixel_position), blur_amount);
            let frosted = tintPremultiplied(backdrop, panel.color.rgb, panel.color.a);
            layer = mix(layer, frosted, coverage);
        }
    }
    return layer;
}
)wgsl";

constexpr char kBlurShader[] = R"wgsl(
struct VertexOut {
    @builtin(position) position : vec4f,
    @location(0) uv : vec2f,
};

struct BlurUniforms {
    source_size : vec4f,
    direction : vec4f,
};

@group(0) @binding(0) var<uniform> blur : BlurUniforms;
@group(0) @binding(1) var source_texture : texture_2d<f32>;
@group(0) @binding(2) var source_sampler : sampler;

@vertex
fn vertexMain(@builtin(vertex_index) index : u32) -> VertexOut {
    let clip = array<vec2f, 6>(
        vec2f(-1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0, -1.0),
        vec2f( 1.0, -1.0),
        vec2f(-1.0,  1.0),
        vec2f( 1.0,  1.0),
    );

    var out : VertexOut;
    out.position = vec4f(clip[index], 0.0, 1.0);
    out.uv = vec2f((clip[index].x + 1.0) * 0.5, (1.0 - clip[index].y) * 0.5);
    return out;
}

fn sampleSource(uv : vec2f) -> vec4f {
    return textureSampleLevel(source_texture, source_sampler, clamp(uv, vec2f(0.0), vec2f(1.0)), 0.0);
}

fn downsample(uv : vec2f) -> vec4f {
    let texel = 1.0 / max(blur.source_size.xy, vec2f(1.0));
    var layer = vec4f(0.0);
    for (var y = 0u; y < 4u; y = y + 1u) {
        for (var x = 0u; x < 4u; x = x + 1u) {
            let offset = vec2f(f32(x) - 1.5, f32(y) - 1.5) * texel;
            layer = layer + sampleSource(uv + offset);
        }
    }
    return layer * 0.0625;
}

fn blur1d(uv : vec2f) -> vec4f {
    let weights = array<f32, 13>(
        0.0185440,
        0.0341660,
        0.0563310,
        0.0831080,
        0.1097190,
        0.1296180,
        0.1370230,
        0.1296180,
        0.1097190,
        0.0831080,
        0.0563310,
        0.0341660,
        0.0185440,
    );
    let radius = max(blur.direction.z, 1.0);
    let step_radius = radius / 6.0;
    var layer = vec4f(0.0);
    for (var index = 0u; index < 13u; index = index + 1u) {
        let offset = blur.direction.xy * ((f32(index) - 6.0) * step_radius);
        layer = layer + sampleSource(uv + offset) * weights[index];
    }
    return layer;
}

@fragment
fn fragmentMain(in : VertexOut) -> @location(0) vec4f {
    if (blur.direction.w < 0.5) {
        return downsample(in.uv);
    }
    return blur1d(in.uv);
}
)wgsl";

struct SymbolButtonUniform {
  float frame[4];
  float icon[4];
  float style[4];
  float control[4];
  float appearance[4];
  float color[4];
  float clip[4];
};

struct PanelUniform {
  float frame[4];
  float color[4];
  float style[4];
  float shadow_color[4];
  float shadow_params[4];
  float clip[4];
};

struct TextUniform {
  float frame[4];
  float uv[4];
  float color[4];
  float clip[4];
};

// The per-frame header (viewport + counts) is a small uniform; the per-kind
// draw records go into storage buffers, so the scene is no longer bounded by
// the old fixed 16/128/128 uniform-array caps.
struct SceneHeader {
  float viewport[4];
  float counts[4];
};

struct EffectPanelUniform {
  float frame[4];
  float color[4];
  float style[4];
  float clip[4];
};

struct EffectUniforms {
  float viewport[4];
  float counts[4];
  EffectPanelUniform effects[kMaxEffectPanelCount];
};

struct BlurUniforms {
  float source_size[4];
  float direction[4];
};

struct PixelRect {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t width = 0;
  uint32_t height = 0;
};

struct TextAtlasEntry {
  LayoutRect frame;
  float uv_left = 0.0f;
  float uv_top = 0.0f;
  float uv_right = 1.0f;
  float uv_bottom = 1.0f;
  phenotype::ui::Color color;
  std::optional<LayoutRect> clip_rect;
};

struct TextAtlas {
  uint32_t width = 1;
  uint32_t height = 1;
  std::vector<uint8_t> pixels = std::vector<uint8_t>(4, 0);
  std::vector<TextAtlasEntry> entries;
  std::vector<TextAtlasEntry> symbol_entries;
};

struct TextAtlasCacheKey {
  size_t text_count = 0;
  size_t symbol_count = 0;
  size_t hash = 0;
};

bool operator==(const TextAtlasCacheKey &lhs, const TextAtlasCacheKey &rhs) noexcept {
  return lhs.text_count == rhs.text_count && lhs.symbol_count == rhs.symbol_count &&
         lhs.hash == rhs.hash;
}

void HashCombine(size_t &seed, size_t value) noexcept {
  seed ^= value + 0x9e3779b97f4a7c15ULL + (seed << 6U) + (seed >> 2U);
}

void HashFloat(size_t &seed, float value) noexcept { HashCombine(seed, std::hash<float>{}(value)); }

void HashInt(size_t &seed, int value) noexcept { HashCombine(seed, std::hash<int>{}(value)); }

void HashBool(size_t &seed, bool value) noexcept { HashCombine(seed, std::hash<bool>{}(value)); }

void HashString(size_t &seed, std::string_view value) noexcept {
  HashCombine(seed, std::hash<std::string_view>{}(value));
}

void HashPixelSize(size_t &seed, float points, float scale) noexcept {
  float pixels = std::max(1.0f, points * scale);
  HashCombine(seed, std::hash<uint32_t>{}(static_cast<uint32_t>(std::ceil(pixels))));
}

void HashTextAtlasFrameSize(size_t &seed, LayoutRect frame, float scale) noexcept {
  HashPixelSize(seed, frame.width, scale);
  HashPixelSize(seed, frame.height, scale);
}

void HashSymbolOptions(size_t &seed, phenotype::ui::SymbolOptions options) noexcept {
  HashBool(seed, options.fill);
  HashFloat(seed, options.weight);
  HashFloat(seed, options.grade);
  HashFloat(seed, options.optical_size);
}

uint32_t PixelSize(CGFloat points, CGFloat scale) noexcept {
  double pixels = static_cast<double>(points * scale);
  if (pixels < 1.0) {
    return 1;
  }
  return static_cast<uint32_t>(pixels);
}

uint32_t AlignUp(uint32_t value, uint32_t alignment) noexcept {
  if (alignment == 0) {
    return value;
  }
  return ((value + alignment - 1) / alignment) * alignment;
}

constexpr uint32_t FontAxisTag(char a, char b, char c, char d) noexcept {
  return (static_cast<uint32_t>(a) << 24U) | (static_cast<uint32_t>(b) << 16U) |
         (static_cast<uint32_t>(c) << 8U) | static_cast<uint32_t>(d);
}

float LogicalPixel(CGFloat points, CGFloat scale) noexcept {
  return static_cast<float>(points * scale);
}

CGFloat ToNativeFontWeight(float weight) noexcept {
  if (weight >= 700.0f) {
    return NSFontWeightBold;
  }
  if (weight >= 600.0f) {
    return NSFontWeightSemibold;
  }
  if (weight >= 500.0f) {
    return NSFontWeightMedium;
  }
  if (weight <= 200.0f) {
    return NSFontWeightUltraLight;
  }
  if (weight <= 300.0f) {
    return NSFontWeightLight;
  }
  return NSFontWeightRegular;
}

NSFont *TextFont(float point_size, float weight) {
  CGFloat size = std::max<CGFloat>(1.0, point_size);
  return [NSFont systemFontOfSize:size weight:ToNativeFontWeight(weight)];
}

std::filesystem::path SourcePretendardFontPath() {
  std::filesystem::path source_file = __FILE__;
  return source_file.parent_path().parent_path().parent_path() / "resources" / "fonts" /
         "PretendardVariable.ttf";
}

std::filesystem::path SourceMaterialSymbolsFontPath() {
  std::filesystem::path source_file = __FILE__;
  return source_file.parent_path().parent_path().parent_path() / "resources" / "fonts" /
         "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
}

std::filesystem::path FindPretendardFontPath() {
  std::vector<std::filesystem::path> candidates;

  NSBundle *bundle = [NSBundle mainBundle];
  NSString *resource_path = [bundle resourcePath];
  if (resource_path != nil) {
    candidates.emplace_back([resource_path UTF8String]);
    candidates.back() /= "Fonts/PretendardVariable.ttf";
  }

  std::filesystem::path current_path = std::filesystem::current_path();
  candidates.push_back(current_path / "macos" / "resources" / "fonts" / "PretendardVariable.ttf");
  candidates.push_back(current_path / "../../../macos/resources/fonts/"
                                      "PretendardVariable.ttf");
  candidates.push_back(SourcePretendardFontPath());

  for (const std::filesystem::path &candidate : candidates) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) {
      return candidate;
    }
  }
  return {};
}

std::filesystem::path FindMaterialSymbolsFontPath() {
  std::vector<std::filesystem::path> candidates;

  NSBundle *bundle = [NSBundle mainBundle];
  NSString *resource_path = [bundle resourcePath];
  if (resource_path != nil) {
    candidates.emplace_back([resource_path UTF8String]);
    candidates.back() /= "Fonts/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf";
  }

  std::filesystem::path current_path = std::filesystem::current_path();
  candidates.push_back(current_path / "macos" / "resources" / "fonts" /
                       "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf");
  candidates.push_back(current_path / "../../../macos/resources/fonts/"
                                      "MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf");
  candidates.push_back(SourceMaterialSymbolsFontPath());

  for (const std::filesystem::path &candidate : candidates) {
    std::error_code error;
    if (std::filesystem::is_regular_file(candidate, error)) {
      return candidate;
    }
  }
  return {};
}

void RegisterPretendardFontIfAvailable() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::filesystem::path font_path = FindPretendardFontPath();
    if (font_path.empty()) {
      std::fprintf(stderr, "phenotype: Pretendard font file not found\n");
      return;
    }

    std::string font_path_text = font_path.string();
    NSString *native_path = [NSString stringWithUTF8String:font_path_text.c_str()];
    if (native_path == nil) {
      return;
    }

    NSURL *font_url = [NSURL fileURLWithPath:native_path];
    CFErrorRef error = nullptr;
    bool registered = CTFontManagerRegisterFontsForURL(
        reinterpret_cast<CFURLRef>(font_url), kCTFontManagerScopeProcess, &error);
    if (!registered && error != nullptr) {
      CFIndex code = CFErrorGetCode(error);
      if (code != kCTFontManagerErrorAlreadyRegistered) {
        std::fprintf(stderr, "phenotype: failed to register Pretendard font (%ld)\n",
            static_cast<long>(code));
      }
      CFRelease(error);
    }
  });
}

void RegisterMaterialSymbolsFontIfAvailable() {
  static std::once_flag once;
  std::call_once(once, [] {
    std::filesystem::path font_path = FindMaterialSymbolsFontPath();
    if (font_path.empty()) {
      std::fprintf(stderr, "phenotype: Material Symbols font file not found\n");
      return;
    }

    std::string font_path_text = font_path.string();
    NSString *native_path = [NSString stringWithUTF8String:font_path_text.c_str()];
    if (native_path == nil) {
      return;
    }

    NSURL *font_url = [NSURL fileURLWithPath:native_path];
    CFErrorRef error = nullptr;
    bool registered = CTFontManagerRegisterFontsForURL(
        reinterpret_cast<CFURLRef>(font_url), kCTFontManagerScopeProcess, &error);
    if (!registered && error != nullptr) {
      CFIndex code = CFErrorGetCode(error);
      if (code != kCTFontManagerErrorAlreadyRegistered) {
        std::fprintf(stderr,
            "phenotype: failed to register Material Symbols font "
            "(%ld)\n",
            static_cast<long>(code));
      }
      CFRelease(error);
    }
  });
}

NSFont *DefaultTextFont(float point_size, float weight) {
  RegisterPretendardFontIfAvailable();

  CGFloat size = std::max<CGFloat>(1.0, point_size);
  NSDictionary *traits = @{
    NSFontWeightTrait : @(ToNativeFontWeight(weight)),
  };

  for (NSString *family_name in @[ @"Pretendard Variable", @"Pretendard" ]) {
    NSDictionary *attributes = @{
      NSFontFamilyAttribute : family_name,
      NSFontTraitsAttribute : traits,
    };
    NSFontDescriptor *descriptor = [NSFontDescriptor fontDescriptorWithFontAttributes:attributes];
    NSFont *font = [NSFont fontWithDescriptor:descriptor size:size];
    if (font != nil && [[font familyName] rangeOfString:@"Pretendard"].location != NSNotFound) {
      return font;
    }
  }

  return TextFont(point_size, weight);
}

NSFont *MaterialSymbolFont(float point_size, phenotype::ui::SymbolOptions options) {
  RegisterMaterialSymbolsFontIfAvailable();

  CGFloat size = std::max<CGFloat>(1.0, point_size);
  NSDictionary *variations = @{
    @(FontAxisTag('F', 'I', 'L', 'L')) : @(options.fill ? 1.0 : 0.0),
    @(FontAxisTag('G', 'R', 'A', 'D')) : @(std::clamp(options.grade, -50.0f, 200.0f)),
    @(FontAxisTag('o', 'p', 's', 'z')) : @(std::clamp(options.optical_size, 20.0f, 48.0f)),
    @(FontAxisTag('w', 'g', 'h', 't')) : @(std::clamp(options.weight, 100.0f, 700.0f)),
  };
  NSDictionary *attributes = @{
    NSFontNameAttribute : @"MaterialSymbolsRounded-Regular",
    NSFontVariationAttribute : variations,
  };
  NSFontDescriptor *descriptor = [NSFontDescriptor fontDescriptorWithFontAttributes:attributes];
  NSFont *font = [NSFont fontWithDescriptor:descriptor size:size];
  if (font != nil) {
    return font;
  }
  return TextFont(point_size, options.weight);
}

std::string_view MaterialSymbolName(phenotype::ui::Symbol symbol) noexcept {
  switch (symbol) {
  case phenotype::ui::Symbol::chevron_left:
    return "chevron_left";
  case phenotype::ui::Symbol::chevron_right:
    return "chevron_right";
  case phenotype::ui::Symbol::folder:
    return "folder";
  case phenotype::ui::Symbol::description:
    return "description";
  case phenotype::ui::Symbol::search:
    return "search";
  case phenotype::ui::Symbol::close:
    return "close";
  }
  return "";
}

TextAtlasCacheKey MakeTextAtlasCacheKey(const std::vector<TextLayout> &texts,
    const std::vector<SymbolButtonLayout> &symbols, float scale) {
  float safe_scale = std::max(1.0f, scale);
  size_t hash = 0;
  HashFloat(hash, safe_scale);

  for (const TextLayout &text : texts) {
    HashString(hash, text.content);
    HashTextAtlasFrameSize(hash, text.frame, safe_scale);
    HashFloat(hash, text.font_size);
    HashFloat(hash, text.font_weight);
    HashInt(hash, text.line_limit);
    HashCombine(hash, static_cast<size_t>(text.overflow));
    HashCombine(hash, static_cast<size_t>(text.truncation));
    HashBool(hash, text.centers_text);
  }

  for (const SymbolButtonLayout &symbol : symbols) {
    HashString(hash, MaterialSymbolName(symbol.symbol));
    HashTextAtlasFrameSize(hash, symbol.frame, safe_scale);
    HashSymbolOptions(hash, symbol.options);
  }

  return {
      texts.size(),
      symbols.size(),
      hash,
  };
}

bool CanRasterizeText(const TextLayout &text, size_t index) noexcept {
  return !text.content.empty() && text.frame.width > 0.0f && text.frame.height > 0.0f &&
         index < kTextAtlasEntryLimit;
}

bool CanRasterizeSymbol(const SymbolButtonLayout &symbol, size_t index) noexcept {
  return !MaterialSymbolName(symbol.symbol).empty() && symbol.frame.width > 0.0f &&
         symbol.frame.height > 0.0f && index < kSymbolAtlasEntryLimit;
}

TextAtlas RetargetTextAtlasEntries(const TextAtlas &cached, const std::vector<TextLayout> &texts,
    const std::vector<SymbolButtonLayout> &symbols) {
  TextAtlas atlas;
  atlas.width = cached.width;
  atlas.height = cached.height;
  atlas.entries = cached.entries;
  atlas.symbol_entries = cached.symbol_entries;

  size_t entry_index = 0;
  for (size_t index = 0; index < texts.size() && entry_index < atlas.entries.size(); ++index) {
    const TextLayout &text = texts[index];
    if (!CanRasterizeText(text, index)) {
      continue;
    }

    TextAtlasEntry &entry = atlas.entries[entry_index++];
    entry.frame = text.frame;
    entry.color = text.color;
    entry.clip_rect = text.clip_rect;
  }

  for (size_t index = 0; index < symbols.size() && index < atlas.symbol_entries.size(); ++index) {
    const SymbolButtonLayout &symbol = symbols[index];
    if (!CanRasterizeSymbol(symbol, index)) {
      continue;
    }

    TextAtlasEntry &entry = atlas.symbol_entries[index];
    entry.frame = symbol.frame;
    entry.color = symbol.color;
    entry.clip_rect = symbol.clip_rect;
  }

  return atlas;
}

NSLineBreakMode ToNativeLineBreakMode(
    phenotype::ui::TextOverflow overflow, phenotype::ui::TextTruncation truncation) noexcept {
  if (overflow != phenotype::ui::TextOverflow::ellipsis) {
    return NSLineBreakByClipping;
  }

  switch (truncation) {
  case phenotype::ui::TextTruncation::head:
    return NSLineBreakByTruncatingHead;
  case phenotype::ui::TextTruncation::middle:
    return NSLineBreakByTruncatingMiddle;
  case phenotype::ui::TextTruncation::tail:
    return NSLineBreakByTruncatingTail;
  }
  return NSLineBreakByTruncatingTail;
}

phenotype::ui::Size MeasureText(std::string_view content, float font_size, float font_weight) {
  if (content.empty()) {
    return {};
  }

  NSString *text = [NSString stringWithUTF8String:std::string(content).c_str()];
  if (text == nil) {
    return {};
  }

  NSFont *font = DefaultTextFont(font_size, font_weight);
  NSDictionary *attributes = @{NSFontAttributeName : font};
  NSSize size = [text sizeWithAttributes:attributes];
  return {
      static_cast<float>(std::ceil(size.width)),
      static_cast<float>(std::ceil(size.height)),
  };
}

TextAtlas BuildTextAtlas(const std::vector<TextLayout> &texts,
    const std::vector<SymbolButtonLayout> &symbols, float scale) {
  enum class PendingKind {
    text,
    symbol,
  };

  struct PendingText {
    PendingKind kind = PendingKind::text;
    size_t index = 0;
    LayoutRect frame;
    std::string content;
    phenotype::ui::Color color;
    float font_size = 17.0f;
    float font_weight = 400.0f;
    int line_limit = 0;
    phenotype::ui::TextOverflow overflow = phenotype::ui::TextOverflow::clip;
    phenotype::ui::TextTruncation truncation = phenotype::ui::TextTruncation::tail;
    phenotype::ui::SymbolOptions symbol_options;
    bool centers_text = false;
    std::optional<LayoutRect> clip_rect;
    uint32_t x = 0;
    uint32_t y = 0;
    uint32_t width = 0;
    uint32_t height = 0;
  };

  constexpr uint32_t max_atlas_width = 2048;
  float safe_scale = std::max(1.0f, scale);
  std::vector<PendingText> pending;
  pending.reserve(texts.size() + symbols.size());

  uint32_t cursor_x = kTextAtlasPadding;
  uint32_t cursor_y = kTextAtlasPadding;
  uint32_t row_height = 0;
  uint32_t atlas_width = 1;
  uint32_t atlas_height = 1;

  auto add_pending = [&](PendingText item) {
    if (item.content.empty() || item.frame.width <= 0.0f || item.frame.height <= 0.0f) {
      return;
    }

    if (item.kind == PendingKind::text && item.index >= kTextAtlasEntryLimit) {
      return;
    }
    if (item.kind == PendingKind::symbol && item.index >= kSymbolAtlasEntryLimit) {
      return;
    }

    uint32_t width =
        std::max<uint32_t>(1, static_cast<uint32_t>(std::ceil(item.frame.width * safe_scale)));
    uint32_t height =
        std::max<uint32_t>(1, static_cast<uint32_t>(std::ceil(item.frame.height * safe_scale)));
    if (cursor_x + width + kTextAtlasPadding > max_atlas_width && cursor_x > kTextAtlasPadding) {
      cursor_x = kTextAtlasPadding;
      cursor_y += row_height + kTextAtlasPadding;
      row_height = 0;
    }

    item.x = cursor_x;
    item.y = cursor_y;
    item.width = width;
    item.height = height;
    pending.push_back(std::move(item));

    cursor_x += width + kTextAtlasPadding;
    row_height = std::max(row_height, height);
    atlas_width = std::max(atlas_width, cursor_x);
    atlas_height = std::max(atlas_height, cursor_y + row_height + kTextAtlasPadding);
  };

  for (size_t index = 0; index < texts.size(); ++index) {
    const TextLayout &text = texts[index];
    if (pending.size() >= kTextAtlasEntryLimit + kSymbolAtlasEntryLimit) {
      continue;
    }

    add_pending({
        .kind = PendingKind::text,
        .index = index,
        .frame = text.frame,
        .content = text.content,
        .color = text.color,
        .font_size = text.font_size,
        .font_weight = text.font_weight,
        .line_limit = text.line_limit,
        .overflow = text.overflow,
        .truncation = text.truncation,
        .centers_text = text.centers_text,
        .clip_rect = text.clip_rect,
    });
  }

  for (size_t index = 0; index < symbols.size(); ++index) {
    if (pending.size() >= kTextAtlasEntryLimit + kSymbolAtlasEntryLimit) {
      continue;
    }

    const SymbolButtonLayout &symbol = symbols[index];
    std::string_view symbol_name = MaterialSymbolName(symbol.symbol);
    add_pending({
        .kind = PendingKind::symbol,
        .index = index,
        .frame = symbol.frame,
        .content = std::string(symbol_name),
        .color = symbol.color,
        .font_size = symbol.options.optical_size,
        .font_weight = symbol.options.weight,
        .symbol_options = symbol.options,
        .centers_text = true,
        .clip_rect = symbol.clip_rect,
    });
  }

  if (pending.empty()) {
    return {};
  }

  TextAtlas atlas;
  atlas.width = AlignUp(atlas_width, kTextAtlasRowAlignmentPixels);
  atlas.height = atlas_height;
  atlas.pixels.assign(static_cast<size_t>(atlas.width) * atlas.height * 4, 0);
  atlas.entries.reserve(pending.size());
  atlas.symbol_entries.resize(symbols.size());

  CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
  CGBitmapInfo bitmap_info = static_cast<CGBitmapInfo>(kCGImageAlphaPremultipliedLast) |
                             static_cast<CGBitmapInfo>(kCGBitmapByteOrder32Big);
  CGContextRef context = CGBitmapContextCreate(
      atlas.pixels.data(), atlas.width, atlas.height, 8, atlas.width * 4, color_space, bitmap_info);
  CGColorSpaceRelease(color_space);
  if (context == nullptr) {
    return {};
  }

  CGContextTranslateCTM(context, 0.0, static_cast<CGFloat>(atlas.height));
  CGContextScaleCTM(context, 1.0, -1.0);

  NSGraphicsContext *graphics_context =
      [NSGraphicsContext graphicsContextWithCGContext:context flipped:YES];
  [NSGraphicsContext saveGraphicsState];
  [NSGraphicsContext setCurrentContext:graphics_context];

  for (const PendingText &text : pending) {
    NSString *content = [NSString stringWithUTF8String:text.content.c_str()];
    if (content == nil) {
      continue;
    }

    bool is_symbol = text.kind == PendingKind::symbol;
    NSFont *font = is_symbol ? MaterialSymbolFont(text.font_size * safe_scale, text.symbol_options)
                             : DefaultTextFont(text.font_size * safe_scale, text.font_weight);
    NSMutableParagraphStyle *paragraph_style = [[NSMutableParagraphStyle alloc] init];
    [paragraph_style setAlignment:text.centers_text ? NSTextAlignmentCenter : NSTextAlignmentLeft];
    if (!is_symbol) {
      [paragraph_style setLineBreakMode:ToNativeLineBreakMode(text.overflow, text.truncation)];
    }
    NSDictionary *attributes = @{
      NSFontAttributeName : font,
      NSLigatureAttributeName : @1,
      NSParagraphStyleAttributeName : paragraph_style,
      NSForegroundColorAttributeName : [NSColor colorWithCalibratedWhite:1.0 alpha:1.0]
    };
    if (is_symbol) {
      NSSize size = [content sizeWithAttributes:attributes];
      CGFloat draw_x =
          static_cast<CGFloat>(text.x) + (static_cast<CGFloat>(text.width) - size.width) * 0.5;
      CGFloat draw_y =
          static_cast<CGFloat>(text.y) + (static_cast<CGFloat>(text.height) - size.height) * 0.5;
      [content drawAtPoint:NSMakePoint(draw_x, draw_y) withAttributes:attributes];
    } else {
      CGFloat draw_height = static_cast<CGFloat>(text.height);
      if (text.line_limit > 0) {
        CGFloat line_height = std::ceil([font ascender] - [font descender] + [font leading]);
        draw_height = std::min(draw_height, line_height * static_cast<CGFloat>(text.line_limit));
      }
      NSStringDrawingOptions draw_options = NSStringDrawingUsesLineFragmentOrigin;
      if (text.overflow == phenotype::ui::TextOverflow::ellipsis) {
        draw_options |= NSStringDrawingTruncatesLastVisibleLine;
      }
      [content drawWithRect:NSMakeRect(text.x, text.y, text.width, draw_height)
                    options:draw_options
                 attributes:attributes];
    }
    [paragraph_style release];

    TextAtlasEntry entry{
        text.frame,
        static_cast<float>(text.x) / static_cast<float>(atlas.width),
        static_cast<float>(text.y) / static_cast<float>(atlas.height),
        static_cast<float>(text.x + text.width) / static_cast<float>(atlas.width),
        static_cast<float>(text.y + text.height) / static_cast<float>(atlas.height),
        text.color,
        text.clip_rect,
    };
    if (is_symbol) {
      if (text.index < atlas.symbol_entries.size()) {
        atlas.symbol_entries[text.index] = entry;
      }
    } else {
      atlas.entries.push_back(entry);
    }
  }

  [NSGraphicsContext restoreGraphicsState];
  CGContextRelease(context);
  return atlas;
}

constexpr std::array<NSWindowButton, 3> LeadingWindowButtonTypes() noexcept {
  return {
      NSWindowCloseButton,
      NSWindowMiniaturizeButton,
      NSWindowZoomButton,
  };
}

NSRect InitialWindowVisibleFrame(CGFloat width, CGFloat height) {
  NSArray<NSScreen *> *screens = [NSScreen screens];
  for (NSScreen *screen in screens) {
    NSRect frame = [screen visibleFrame];
    if (frame.origin.x == 0.0 && frame.origin.y == 0.0) {
      return frame;
    }
  }

  NSPoint mouse = [NSEvent mouseLocation];
  for (NSScreen *screen in screens) {
    if (NSPointInRect(mouse, [screen frame])) {
      return [screen visibleFrame];
    }
  }

  NSScreen *screen = [NSScreen mainScreen];
  if (screen) {
    return [screen visibleFrame];
  }
  return NSMakeRect(0.0, 0.0, width, height);
}

NSVisualEffectMaterial ToNativeVisualMaterial(phenotype::macos::window::VisualMaterial material) {
  switch (material) {
  case phenotype::macos::window::VisualMaterial::under_window_background:
    return NSVisualEffectMaterialUnderWindowBackground;
  }
  return NSVisualEffectMaterialUnderWindowBackground;
}

void ApplyTitleBarStyle(NSWindow *window, phenotype::macos::window::TitleBarStyle style) {
  if (style == phenotype::macos::window::TitleBarStyle::hidden) {
    [window setStyleMask:[window styleMask] | NSWindowStyleMaskFullSizeContentView];
    [window setTitleVisibility:NSWindowTitleHidden];
    [window setTitlebarAppearsTransparent:YES];
    [window setMovableByWindowBackground:NO];
    return;
  }

  [window setTitleVisibility:NSWindowTitleVisible];
  [window setTitlebarAppearsTransparent:NO];
  [window setMovableByWindowBackground:NO];
}

bool CaptureWindowControlFrames(
    NSWindow *window, std::array<NSRect, 3> &frames, std::array<bool, 3> &has_frame) {
  frames.fill(NSZeroRect);
  has_frame.fill(false);
  if (!window) {
    return false;
  }

  bool has_any_frame = false;
  std::array<NSWindowButton, 3> button_types = LeadingWindowButtonTypes();
  for (size_t index = 0; index < button_types.size(); ++index) {
    NSButton *button = [window standardWindowButton:button_types[index]];
    if (!button || ![button superview]) {
      continue;
    }

    [[button superview] layoutSubtreeIfNeeded];
    frames[index] = [button frame];
    has_frame[index] = true;
    has_any_frame = true;
  }
  return has_any_frame;
}

// Merge the scene contract's NearlyEqual overloads (Size, LayoutRect,
// LayoutContext, ...) with the AppKit-specific CGFloat/NSRect ones below so
// unqualified calls resolve against the whole set instead of being hidden.
using phenotype::scene::NearlyEqual;

bool NearlyEqual(CGFloat lhs, CGFloat rhs) noexcept { return std::abs(lhs - rhs) < 0.5; }

bool NearlyEqual(NSRect lhs, NSRect rhs) noexcept {
  return NearlyEqual(lhs.origin.x, rhs.origin.x) && NearlyEqual(lhs.origin.y, rhs.origin.y) &&
         NearlyEqual(lhs.size.width, rhs.size.width) &&
         NearlyEqual(lhs.size.height, rhs.size.height);
}

void ApplyWindowControlVerticalOffset(NSWindow *window, const std::array<NSRect, 3> &base_frames,
    const std::array<bool, 3> &has_base_frame, CGFloat offset) {
  if (!window) {
    return;
  }

  std::array<NSWindowButton, 3> button_types = LeadingWindowButtonTypes();
  for (size_t index = 0; index < button_types.size(); ++index) {
    if (!has_base_frame[index]) {
      continue;
    }

    NSButton *button = [window standardWindowButton:button_types[index]];
    if (!button || ![button superview]) {
      continue;
    }

    NSRect frame = base_frames[index];
    frame.origin.y -= offset;
    if (!NearlyEqual([button frame], frame)) {
      [button setFrame:frame];
    }
  }
}

LayoutContext BuildLayoutContext(NSWindow *window, NSView *content_view) {
  LayoutContext context;
  if (!window || !content_view) {
    return context;
  }

  bool has_controls = false;
  NSRect controls_rect = NSZeroRect;
  for (NSWindowButton button_type : LeadingWindowButtonTypes()) {
    NSButton *button = [window standardWindowButton:button_type];
    if (!button || [button isHidden] || ![button superview]) {
      continue;
    }

    NSRect button_rect = [button convertRect:[button bounds] toView:nil];
    button_rect = [content_view convertRect:button_rect fromView:nil];
    controls_rect = has_controls ? NSUnionRect(controls_rect, button_rect) : button_rect;
    has_controls = true;
  }

  if (!has_controls) {
    return context;
  }

  NSRect content_bounds = [content_view bounds];
  context.window_controls.has_leading_controls = true;
  context.window_controls.leading_controls = {
      static_cast<float>(NSMinX(controls_rect)),
      static_cast<float>(NSHeight(content_bounds) - NSMaxY(controls_rect)),
      static_cast<float>(NSWidth(controls_rect)),
      static_cast<float>(NSHeight(controls_rect)),
  };
  return context;
}

bool HasArea(PixelRect rect) noexcept { return rect.width > 0 && rect.height > 0; }

PixelRect ScaleToPixelRect(
    LayoutRect rect, float scale, uint32_t width, uint32_t height, float padding) noexcept {
  if (width == 0 || height == 0) {
    return {};
  }

  float min_x = (rect.x - padding) * scale;
  float min_y = (rect.y - padding) * scale;
  float max_x = (rect.x + rect.width + padding) * scale;
  float max_y = (rect.y + rect.height + padding) * scale;

  int32_t x0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(min_x)));
  int32_t y0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(min_y)));
  int32_t x1 =
      std::min<int32_t>(static_cast<int32_t>(width), static_cast<int32_t>(std::ceil(max_x)));
  int32_t y1 =
      std::min<int32_t>(static_cast<int32_t>(height), static_cast<int32_t>(std::ceil(max_y)));
  if (x1 <= x0 || y1 <= y0) {
    return {};
  }
  return {
      static_cast<uint32_t>(x0),
      static_cast<uint32_t>(y0),
      static_cast<uint32_t>(x1 - x0),
      static_cast<uint32_t>(y1 - y0),
  };
}

PixelRect ScalePixelRect(PixelRect rect, uint32_t source_width, uint32_t source_height,
    uint32_t target_width, uint32_t target_height) noexcept {
  if (!HasArea(rect) || source_width == 0 || source_height == 0 || target_width == 0 ||
      target_height == 0) {
    return {};
  }

  float scale_x = static_cast<float>(target_width) / static_cast<float>(source_width);
  float scale_y = static_cast<float>(target_height) / static_cast<float>(source_height);
  int32_t x0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(rect.x * scale_x)));
  int32_t y0 = std::max<int32_t>(0, static_cast<int32_t>(std::floor(rect.y * scale_y)));
  int32_t x1 = std::min<int32_t>(static_cast<int32_t>(target_width),
      static_cast<int32_t>(std::ceil((rect.x + rect.width) * scale_x)));
  int32_t y1 = std::min<int32_t>(static_cast<int32_t>(target_height),
      static_cast<int32_t>(std::ceil((rect.y + rect.height) * scale_y)));
  if (x1 <= x0 || y1 <= y0) {
    return {};
  }
  return {
      static_cast<uint32_t>(x0),
      static_cast<uint32_t>(y0),
      static_cast<uint32_t>(x1 - x0),
      static_cast<uint32_t>(y1 - y0),
  };
}

void FillClipUniform(float values[4], std::optional<LayoutRect> clip_rect, LayoutRect fallback,
    float scale) noexcept {
  LayoutRect clip = clip_rect.value_or(fallback);
  values[0] = clip.x * scale;
  values[1] = clip.y * scale;
  values[2] = clip.width * scale;
  values[3] = clip.height * scale;
}

SymbolButtonUniform MakeSymbolButton(const SymbolButtonLayout &button, const TextAtlasEntry &symbol,
    float scale, LayoutRect fallback_clip) {
  float alpha_scale = button.is_enabled ? 1.0f : 0.44f;
  float safe_scale = std::max(1.0f, scale);
  SymbolButtonUniform uniform{
      {
          (button.frame.x + (button.frame.width * 0.5f)) * safe_scale,
          (button.frame.y + (button.frame.height * 0.5f)) * safe_scale,
          button.frame.width * safe_scale,
          button.frame.height * safe_scale,
      },
      {
          symbol.uv_left,
          symbol.uv_top,
          symbol.uv_right,
          symbol.uv_bottom,
      },
      {
          button.options.fill ? 1.0f : 0.0f,
          button.options.weight,
          button.options.grade,
          button.options.optical_size * safe_scale,
      },
      {
          (button.control_frame.x + (button.control_frame.width * 0.5f)) * safe_scale,
          (button.control_frame.y + (button.control_frame.height * 0.5f)) * safe_scale,
          button.control_frame.width * safe_scale,
          button.control_frame.height * safe_scale,
      },
      {
          ControlShapeValue(button.control_shape),
          button.draws_control ? 1.0f : 0.0f,
          button.divider_x * safe_scale,
          button.draws_divider ? 1.0f : 0.0f,
      },
      {
          button.color.red,
          button.color.green,
          button.color.blue,
          button.color.alpha * alpha_scale,
      },
  };
  FillClipUniform(uniform.clip, button.clip_rect, fallback_clip, safe_scale);
  return uniform;
}

PanelUniform MakePanel(const PanelLayout &panel, float scale, LayoutRect fallback_clip) {
  float safe_scale = std::max(1.0f, scale);
  PanelUniform uniform{
      {
          (panel.frame.x + (panel.frame.width * 0.5f)) * safe_scale,
          (panel.frame.y + (panel.frame.height * 0.5f)) * safe_scale,
          panel.frame.width * safe_scale,
          panel.frame.height * safe_scale,
      },
      {
          panel.color.red,
          panel.color.green,
          panel.color.blue,
          panel.color.alpha,
      },
      {
          panel.corner_radius,
          CornerMode(panel.rounds_top_corners_only, panel.rounds_bottom_corners_only),
          0.0f,
          0.0f,
      },
      {
          panel.shadow.color.red,
          panel.shadow.color.green,
          panel.shadow.color.blue,
          panel.shadow.color.alpha,
      },
      {
          panel.shadow.offset.width * safe_scale,
          panel.shadow.offset.height * safe_scale,
          panel.shadow.blur_radius * safe_scale,
          0.0f,
      },
  };
  FillClipUniform(uniform.clip, panel.clip_rect, fallback_clip, safe_scale);
  return uniform;
}

EffectPanelUniform MakeEffectPanel(
    const EffectPanelLayout &panel, float scale, LayoutRect fallback_clip) {
  float safe_scale = std::max(1.0f, scale);
  EffectPanelUniform uniform{
      {
          (panel.frame.x + (panel.frame.width * 0.5f)) * safe_scale,
          (panel.frame.y + (panel.frame.height * 0.5f)) * safe_scale,
          panel.frame.width * safe_scale,
          panel.frame.height * safe_scale,
      },
      {
          panel.color.red,
          panel.color.green,
          panel.color.blue,
          panel.color.alpha,
      },
      {
          panel.corner_radius,
          CornerMode(panel.rounds_top_corners_only, panel.rounds_bottom_corners_only),
          panel.blur_amount,
          0.0f,
      },
  };
  FillClipUniform(uniform.clip, panel.clip_rect, fallback_clip, safe_scale);
  return uniform;
}

TextUniform MakeText(const TextAtlasEntry &text, float scale, LayoutRect fallback_clip) {
  float safe_scale = std::max(1.0f, scale);
  TextUniform uniform{
      {
          (text.frame.x + (text.frame.width * 0.5f)) * safe_scale,
          (text.frame.y + (text.frame.height * 0.5f)) * safe_scale,
          text.frame.width * safe_scale,
          text.frame.height * safe_scale,
      },
      {
          text.uv_left,
          text.uv_top,
          text.uv_right,
          text.uv_bottom,
      },
      {
          text.color.red,
          text.color.green,
          text.color.blue,
          text.color.alpha,
      },
  };
  FillClipUniform(uniform.clip, text.clip_rect, fallback_clip, safe_scale);
  return uniform;
}

void AppendTextLayouts(
    std::vector<TextLayout> &destination, const std::vector<TextLayout> &source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

void AppendButtonLayouts(
    std::vector<SymbolButtonLayout> &destination, const std::vector<SymbolButtonLayout> &source) {
  destination.insert(destination.end(), source.begin(), source.end());
}

// The per-kind draw records for one layer, destined for storage buffers.
struct SceneRecords {
  std::vector<PanelUniform> panels;
  std::vector<SymbolButtonUniform> buttons;
  std::vector<TextUniform> texts;
};

void FillSceneRecords(SceneHeader &header, SceneRecords &records, const SceneDrawLayer &layer,
    const TextAtlas &text_atlas, size_t text_offset, size_t symbol_offset, float width,
    float height, float scale, LayoutRect viewport_clip) {
  header.viewport[0] = width;
  header.viewport[1] = height;
  header.viewport[3] = std::max(1.0f, scale);
  header.counts[0] = static_cast<float>(layer.panels.size());
  header.counts[1] = static_cast<float>(layer.buttons.size());
  header.counts[2] = static_cast<float>(layer.texts.size());

  records.panels.clear();
  records.panels.reserve(layer.panels.size());
  for (size_t index = 0; index < layer.panels.size(); ++index) {
    records.panels.push_back(MakePanel(layer.panels[index], scale, viewport_clip));
  }
  TextAtlasEntry empty_symbol;
  records.buttons.clear();
  records.buttons.reserve(layer.buttons.size());
  for (size_t index = 0; index < layer.buttons.size(); ++index) {
    size_t symbol_index = symbol_offset + index;
    const TextAtlasEntry &symbol = symbol_index < text_atlas.symbol_entries.size()
                                       ? text_atlas.symbol_entries[symbol_index]
                                       : empty_symbol;
    records.buttons.push_back(MakeSymbolButton(layer.buttons[index], symbol, scale, viewport_clip));
  }
  TextAtlasEntry empty_text;
  records.texts.clear();
  records.texts.reserve(layer.texts.size());
  for (size_t index = 0; index < layer.texts.size(); ++index) {
    size_t text_index = text_offset + index;
    const TextAtlasEntry &text =
        text_index < text_atlas.entries.size() ? text_atlas.entries[text_index] : empty_text;
    records.texts.push_back(MakeText(text, scale, viewport_clip));
  }
}

class DawnButtonRenderer {
public:
  // One scene layer's GPU buffers: a small header uniform plus three growable
  // storage buffers (panels / buttons / texts). The bind group is rebuilt
  // whenever any storage buffer is reallocated to a larger capacity.
  struct SceneLayerBuffers {
    wgpu::Buffer header;
    wgpu::Buffer panels;
    wgpu::Buffer buttons;
    wgpu::Buffer texts;
    size_t panel_capacity = 0;
    size_t button_capacity = 0;
    size_t text_capacity = 0;
    // Record counts uploaded this frame, i.e. the instance counts to draw.
    uint32_t panel_count = 0;
    uint32_t button_count = 0;
    uint32_t text_count = 0;
    wgpu::BindGroup bind_group;
  };

  bool Initialize(CAMetalLayer *layer, uint32_t width, uint32_t height, float scale,
      phenotype::ui::Size layout_size, LayoutContext layout_context,
      phenotype::ui::View root_view) {
    _root_view = std::move(root_view);
    _scale = scale;
    _layout_size = layout_size;
    _layout_context = layout_context;

    wgpu::InstanceFeatureName required_features[] = {
        wgpu::InstanceFeatureName::TimedWaitAny,
    };
    wgpu::InstanceDescriptor instance_descriptor;
    instance_descriptor.requiredFeatureCount = 1;
    instance_descriptor.requiredFeatures = required_features;

    _instance = wgpu::CreateInstance(&instance_descriptor);
    if (!_instance) {
      std::fprintf(stderr, "Dawn: failed to create WebGPU instance\n");
      return false;
    }

    wgpu::SurfaceSourceMetalLayer metal_layer_source;
    metal_layer_source.layer = layer;

    wgpu::SurfaceDescriptor surface_descriptor;
    surface_descriptor.nextInChain = &metal_layer_source;
    _surface = _instance.CreateSurface(&surface_descriptor);
    if (!_surface) {
      std::fprintf(stderr, "Dawn: failed to create Metal surface\n");
      return false;
    }

    if (!RequestAdapter() || !RequestDevice()) {
      return false;
    }

    CreateSceneUniformBuffer();
    CreateTextSampler();
    CreateTextTexture(1, 1);
    if (!ConfigureSurface(width, height)) {
      return false;
    }

    CreatePipeline();
    return static_cast<bool>(_panel_pipeline) && static_cast<bool>(_button_pipeline) &&
           static_cast<bool>(_text_pipeline) && static_cast<bool>(_effect_pipeline) &&
           static_cast<bool>(_blur_pipeline);
  }

  void Resize(uint32_t width, uint32_t height, float scale, phenotype::ui::Size layout_size,
      LayoutContext layout_context) {
    if (width == 0 || height == 0 || !_device) {
      return;
    }
    bool size_changed = width != _width || height != _height;
    bool layout_changed = !NearlyEqual(_scale, scale) || !NearlyEqual(_layout_size, layout_size) ||
                          !NearlyEqual(_layout_context, layout_context);
    _scale = scale;
    _layout_size = layout_size;
    _layout_context = layout_context;
    if (!size_changed) {
      if (layout_changed) {
        UpdateSceneUniforms();
        UpdateBlurUniforms();
      }
      return;
    }
    ConfigureSurface(width, height);
  }

  void UpdateRootView(phenotype::ui::View root_view) {
    _root_view = std::move(root_view);
    UpdateSceneUniforms();
  }

  bool ActivateAt(phenotype::ui::Size point) {
    for (auto iterator = _hit_targets.rbegin(); iterator != _hit_targets.rend(); ++iterator) {
      if (!iterator->is_enabled || !Contains(iterator->frame, point) ||
          (iterator->clip_rect && !Contains(*iterator->clip_rect, point))) {
        continue;
      }
      iterator->action();
      return true;
    }
    return false;
  }

  // True when a focused text field is accepting input this frame; the shell uses
  // it to decide whether to consume key events as text.
  bool HasTextInput() const { return !_text_input_targets.empty(); }

  // Deliver an edit command to the focused text field (the last emitted wins if
  // several were focused). Returns true if a field consumed it.
  bool DispatchTextEdit(const phenotype::ui::TextEdit &edit) {
    if (_text_input_targets.empty()) {
      return false;
    }
    _text_input_targets.back().action(edit);
    return true;
  }

  // The focused field's committed text + selection (byte offsets), for clipboard
  // copy/cut. Returns false when nothing is focused.
  bool FocusedSelection(std::string &out_text, size_t &out_begin, size_t &out_end) const {
    if (_text_input_targets.empty()) {
      return false;
    }
    const TextInputTargetLayout &target = _text_input_targets.back();
    out_text = target.text;
    out_begin = target.selection_begin;
    out_end = target.selection_end;
    return true;
  }

  // The focused field's caret rect (field/scene coordinates), for placing the
  // IME candidate window. Returns false when nothing is focused.
  bool FocusedCaretRect(LayoutRect &out_rect) const {
    if (_text_input_targets.empty()) {
      return false;
    }
    out_rect = _text_input_targets.back().caret_rect;
    return true;
  }

  bool HasScrollTargetAt(phenotype::ui::Size point) const {
    for (auto iterator = _scroll_targets.rbegin(); iterator != _scroll_targets.rend(); ++iterator) {
      if (Contains(iterator->frame, point)) {
        return true;
      }
    }
    return false;
  }

  bool ScrollAt(phenotype::ui::Size point, float delta_y) {
    if (delta_y == 0.0f) {
      return false;
    }

    for (auto iterator = _scroll_targets.rbegin(); iterator != _scroll_targets.rend(); ++iterator) {
      if (!Contains(iterator->frame, point)) {
        continue;
      }

      float previous = std::clamp(iterator->offset_y, 0.0f, iterator->max_offset_y);
      float next = std::clamp(previous - delta_y, 0.0f, iterator->max_offset_y);
      if (next != previous) {
        iterator->action(next);
        return true;
      }
      return false;
    }
    return false;
  }

  void Render() {
    if (!_device || !_surface || !_panel_pipeline || !_button_pipeline || !_text_pipeline ||
        !_effect_pipeline || !_blur_pipeline || !_scene_texture_view || !_blur_texture_a_view ||
        !_blur_texture_b_view || !_background_buffers.bind_group ||
        !_foreground_buffers.bind_group || !_effect_bind_group || !_downsample_bind_group ||
        !_horizontal_blur_bind_group || !_vertical_blur_bind_group) {
      return;
    }

    wgpu::SurfaceTexture surface_texture;
    _surface.GetCurrentTexture(&surface_texture);
    if (surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessOptimal &&
        surface_texture.status != wgpu::SurfaceGetCurrentTextureStatus::SuccessSuboptimal) {
      return;
    }

    wgpu::TextureView backbuffer = surface_texture.texture.CreateView();

    wgpu::CommandEncoder encoder = _device.CreateCommandEncoder();
    DrawSceneLayer(encoder, _scene_texture_view, _background_buffers, wgpu::LoadOp::Clear);
    if (_has_effect_panels) {
      DrawFullscreenPass(encoder, _blur_texture_a_view, _blur_pipeline, _downsample_bind_group,
          wgpu::LoadOp::Clear, _blur_scissor);
      DrawFullscreenPass(encoder, _blur_texture_b_view, _blur_pipeline, _horizontal_blur_bind_group,
          wgpu::LoadOp::Clear, _blur_scissor);
      DrawFullscreenPass(encoder, _blur_texture_a_view, _blur_pipeline, _vertical_blur_bind_group,
          wgpu::LoadOp::Clear, _blur_scissor);
    }
    DrawFullscreenPass(
        encoder, backbuffer, _effect_pipeline, _effect_bind_group, wgpu::LoadOp::Clear);
    DrawSceneLayer(encoder, backbuffer, _foreground_buffers, wgpu::LoadOp::Load);

    wgpu::CommandBuffer commands = encoder.Finish();
    _device.GetQueue().Submit(1, &commands);
    _surface.Present();
    _instance.ProcessEvents();
  }

private:
  void DrawFullscreenPass(wgpu::CommandEncoder &encoder, wgpu::TextureView view,
      wgpu::RenderPipeline pipeline, wgpu::BindGroup bind_group, wgpu::LoadOp load_op,
      std::optional<PixelRect> scissor = std::nullopt) {
    if (scissor && !HasArea(*scissor)) {
      return;
    }

    wgpu::RenderPassColorAttachment color_attachment;
    color_attachment.view = view;
    color_attachment.loadOp = load_op;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    if (load_op == wgpu::LoadOp::Clear) {
      color_attachment.clearValue = {0.0, 0.0, 0.0, 0.0};
    }

    wgpu::RenderPassDescriptor render_pass_descriptor;
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_descriptor);
    if (scissor) {
      pass.SetScissorRect(scissor->x, scissor->y, scissor->width, scissor->height);
    }
    pass.SetPipeline(pipeline);
    pass.SetBindGroup(0, bind_group);
    pass.Draw(6);
    pass.End();
  }

  // Draw one scene layer as three instanced passes (panels, then buttons, then
  // texts) into a single render pass, preserving the old compositing order. Each
  // record becomes one 6-vertex quad instance; the premultiplied-over blend
  // composites them exactly as the former single fullscreen loop did.
  void DrawSceneLayer(wgpu::CommandEncoder &encoder, wgpu::TextureView view,
      const SceneLayerBuffers &buffers, wgpu::LoadOp load_op) {
    wgpu::RenderPassColorAttachment color_attachment;
    color_attachment.view = view;
    color_attachment.loadOp = load_op;
    color_attachment.storeOp = wgpu::StoreOp::Store;
    if (load_op == wgpu::LoadOp::Clear) {
      color_attachment.clearValue = {0.0, 0.0, 0.0, 0.0};
    }

    wgpu::RenderPassDescriptor render_pass_descriptor;
    render_pass_descriptor.colorAttachmentCount = 1;
    render_pass_descriptor.colorAttachments = &color_attachment;

    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&render_pass_descriptor);
    pass.SetBindGroup(0, buffers.bind_group);
    if (buffers.panel_count > 0) {
      pass.SetPipeline(_panel_pipeline);
      pass.Draw(6, buffers.panel_count);
    }
    if (buffers.button_count > 0) {
      pass.SetPipeline(_button_pipeline);
      pass.Draw(6, buffers.button_count);
    }
    if (buffers.text_count > 0) {
      pass.SetPipeline(_text_pipeline);
      pass.Draw(6, buffers.text_count);
    }
    pass.End();
  }

  bool RequestAdapter() {
    wgpu::RequestAdapterOptions options;
    options.compatibleSurface = _surface;

    bool resolved = false;
    bool succeeded = false;
    wgpu::Future future = _instance.RequestAdapter(&options, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message) {
          (void)message;
          resolved = true;
          succeeded = status == wgpu::RequestAdapterStatus::Success;
          if (succeeded) {
            _adapter = adapter;
          }
        });

    _instance.WaitAny(future, UINT64_MAX);
    if (!resolved || !succeeded || !_adapter) {
      std::fprintf(stderr, "Dawn: failed to request adapter\n");
      return false;
    }
    return true;
  }

  bool RequestDevice() {
    wgpu::DeviceDescriptor descriptor;
    descriptor.SetUncapturedErrorCallback(
        [](const wgpu::Device &, wgpu::ErrorType error_type, wgpu::StringView message) {
          std::fprintf(stderr, "Dawn device error (%u): %.*s\n", static_cast<unsigned>(error_type),
              static_cast<int>(message.length), message.data);
        });

    bool resolved = false;
    bool succeeded = false;
    wgpu::Future future = _adapter.RequestDevice(&descriptor, wgpu::CallbackMode::WaitAnyOnly,
        [&](wgpu::RequestDeviceStatus status, wgpu::Device device, wgpu::StringView message) {
          (void)message;
          resolved = true;
          succeeded = status == wgpu::RequestDeviceStatus::Success;
          if (succeeded) {
            _device = device;
          }
        });

    _instance.WaitAny(future, UINT64_MAX);
    if (!resolved || !succeeded || !_device) {
      std::fprintf(stderr, "Dawn: failed to request device\n");
      return false;
    }
    return true;
  }

  bool ConfigureSurface(uint32_t width, uint32_t height) {
    wgpu::SurfaceCapabilities capabilities;
    _surface.GetCapabilities(_adapter, &capabilities);
    if (capabilities.formatCount == 0) {
      std::fprintf(stderr, "Dawn: surface returned no supported formats\n");
      return false;
    }

    _format = capabilities.formats[0];
    _width = width;
    _height = height;
    CreateSceneTexture();
    UpdateBlurUniforms();
    UpdateSceneUniforms();

    wgpu::SurfaceConfiguration configuration;
    configuration.device = _device;
    configuration.format = _format;
    configuration.usage = wgpu::TextureUsage::RenderAttachment;
    configuration.width = _width;
    configuration.height = _height;
    configuration.presentMode = wgpu::PresentMode::Fifo;
    configuration.alphaMode = wgpu::CompositeAlphaMode::Premultiplied;
    _surface.Configure(&configuration);
    return true;
  }

  void CreatePipeline() {
    CreateScenePipeline();
    CreateEffectPipeline();
    CreateBlurPipeline();
    CreateSceneBindGroups();
    CreateBlurBindGroups();
    CreateEffectBindGroup();
  }

  void CreateScenePipeline() {
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = kButtonShader;

    std::array<wgpu::BindGroupLayoutEntry, 6> bind_group_layout_entries{};
    bind_group_layout_entries[0].binding = 0;
    bind_group_layout_entries[0].visibility =
        wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    bind_group_layout_entries[0].buffer.minBindingSize = sizeof(SceneHeader);

    // Per-kind draw records as read-only storage buffers (panels/buttons/texts).
    // Visible to the vertex stage too: the instanced vertex shaders read each
    // record's rect to size its quad.
    for (uint32_t storage_index = 1; storage_index <= 3; ++storage_index) {
      bind_group_layout_entries[storage_index].binding = storage_index;
      bind_group_layout_entries[storage_index].visibility =
          wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
      bind_group_layout_entries[storage_index].buffer.type =
          wgpu::BufferBindingType::ReadOnlyStorage;
    }

    bind_group_layout_entries[4].binding = 4;
    bind_group_layout_entries[4].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[4].texture.sampleType = wgpu::TextureSampleType::Float;
    bind_group_layout_entries[4].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    bind_group_layout_entries[5].binding = 5;
    bind_group_layout_entries[5].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[5].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor;
    bind_group_layout_descriptor.entryCount = bind_group_layout_entries.size();
    bind_group_layout_descriptor.entries = bind_group_layout_entries.data();
    _bind_group_layout = _device.CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor;
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout =
        _device.CreatePipelineLayout(&pipeline_layout_descriptor);

    wgpu::ShaderModuleDescriptor shader_descriptor;
    shader_descriptor.nextInChain = &wgsl;
    wgpu::ShaderModule shader = _device.CreateShaderModule(&shader_descriptor);

    wgpu::ColorTargetState color_target;
    color_target.format = _format;
    color_target.writeMask = wgpu::ColorWriteMask::All;
    wgpu::BlendComponent color_blend;
    color_blend.operation = wgpu::BlendOperation::Add;
    color_blend.srcFactor = wgpu::BlendFactor::One;
    color_blend.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    wgpu::BlendComponent alpha_blend;
    alpha_blend.operation = wgpu::BlendOperation::Add;
    alpha_blend.srcFactor = wgpu::BlendFactor::One;
    alpha_blend.dstFactor = wgpu::BlendFactor::OneMinusSrcAlpha;
    wgpu::BlendState blend_state;
    blend_state.color = color_blend;
    blend_state.alpha = alpha_blend;
    color_target.blend = &blend_state;

    // One instanced pipeline per kind, sharing the layout, shader, and
    // premultiplied-over blend. Each draws a quad per record (6 verts × N
    // instances) using the kind's vertex/fragment entry points.
    auto make_pipeline = [&](const char *vertex_entry, const char *fragment_entry) {
      wgpu::FragmentState fragment;
      fragment.module = shader;
      fragment.entryPoint = fragment_entry;
      fragment.targetCount = 1;
      fragment.targets = &color_target;

      wgpu::RenderPipelineDescriptor pipeline_descriptor;
      pipeline_descriptor.layout = pipeline_layout;
      pipeline_descriptor.vertex.module = shader;
      pipeline_descriptor.vertex.entryPoint = vertex_entry;
      pipeline_descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
      pipeline_descriptor.fragment = &fragment;
      pipeline_descriptor.multisample.count = 1;
      return _device.CreateRenderPipeline(&pipeline_descriptor);
    };

    _panel_pipeline = make_pipeline("panelVertex", "panelFragment");
    _button_pipeline = make_pipeline("buttonVertex", "buttonFragment");
    _text_pipeline = make_pipeline("textVertex", "textFragment");
  }

  void CreateEffectPipeline() {
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = kEffectShader;

    std::array<wgpu::BindGroupLayoutEntry, 4> bind_group_layout_entries{};
    bind_group_layout_entries[0].binding = 0;
    bind_group_layout_entries[0].visibility =
        wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    bind_group_layout_entries[0].buffer.minBindingSize = sizeof(EffectUniforms);

    bind_group_layout_entries[1].binding = 1;
    bind_group_layout_entries[1].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[1].texture.sampleType = wgpu::TextureSampleType::Float;
    bind_group_layout_entries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    bind_group_layout_entries[2].binding = 2;
    bind_group_layout_entries[2].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[2].texture.sampleType = wgpu::TextureSampleType::Float;
    bind_group_layout_entries[2].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    bind_group_layout_entries[3].binding = 3;
    bind_group_layout_entries[3].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[3].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor;
    bind_group_layout_descriptor.entryCount = bind_group_layout_entries.size();
    bind_group_layout_descriptor.entries = bind_group_layout_entries.data();
    _effect_bind_group_layout = _device.CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor;
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &_effect_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout =
        _device.CreatePipelineLayout(&pipeline_layout_descriptor);

    wgpu::ShaderModuleDescriptor shader_descriptor;
    shader_descriptor.nextInChain = &wgsl;
    wgpu::ShaderModule shader = _device.CreateShaderModule(&shader_descriptor);

    wgpu::ColorTargetState color_target;
    color_target.format = _format;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fragmentMain";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::RenderPipelineDescriptor pipeline_descriptor;
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.vertex.module = shader;
    pipeline_descriptor.vertex.entryPoint = "vertexMain";
    pipeline_descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipeline_descriptor.fragment = &fragment;
    pipeline_descriptor.multisample.count = 1;

    _effect_pipeline = _device.CreateRenderPipeline(&pipeline_descriptor);
  }

  void CreateBlurPipeline() {
    wgpu::ShaderSourceWGSL wgsl;
    wgsl.code = kBlurShader;

    std::array<wgpu::BindGroupLayoutEntry, 3> bind_group_layout_entries{};
    bind_group_layout_entries[0].binding = 0;
    bind_group_layout_entries[0].visibility =
        wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[0].buffer.type = wgpu::BufferBindingType::Uniform;
    bind_group_layout_entries[0].buffer.minBindingSize = sizeof(BlurUniforms);

    bind_group_layout_entries[1].binding = 1;
    bind_group_layout_entries[1].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[1].texture.sampleType = wgpu::TextureSampleType::Float;
    bind_group_layout_entries[1].texture.viewDimension = wgpu::TextureViewDimension::e2D;

    bind_group_layout_entries[2].binding = 2;
    bind_group_layout_entries[2].visibility = wgpu::ShaderStage::Fragment;
    bind_group_layout_entries[2].sampler.type = wgpu::SamplerBindingType::Filtering;

    wgpu::BindGroupLayoutDescriptor bind_group_layout_descriptor;
    bind_group_layout_descriptor.entryCount = bind_group_layout_entries.size();
    bind_group_layout_descriptor.entries = bind_group_layout_entries.data();
    _blur_bind_group_layout = _device.CreateBindGroupLayout(&bind_group_layout_descriptor);

    wgpu::PipelineLayoutDescriptor pipeline_layout_descriptor;
    pipeline_layout_descriptor.bindGroupLayoutCount = 1;
    pipeline_layout_descriptor.bindGroupLayouts = &_blur_bind_group_layout;
    wgpu::PipelineLayout pipeline_layout =
        _device.CreatePipelineLayout(&pipeline_layout_descriptor);

    wgpu::ShaderModuleDescriptor shader_descriptor;
    shader_descriptor.nextInChain = &wgsl;
    wgpu::ShaderModule shader = _device.CreateShaderModule(&shader_descriptor);

    wgpu::ColorTargetState color_target;
    color_target.format = _format;
    color_target.writeMask = wgpu::ColorWriteMask::All;

    wgpu::FragmentState fragment;
    fragment.module = shader;
    fragment.entryPoint = "fragmentMain";
    fragment.targetCount = 1;
    fragment.targets = &color_target;

    wgpu::RenderPipelineDescriptor pipeline_descriptor;
    pipeline_descriptor.layout = pipeline_layout;
    pipeline_descriptor.vertex.module = shader;
    pipeline_descriptor.vertex.entryPoint = "vertexMain";
    pipeline_descriptor.primitive.topology = wgpu::PrimitiveTopology::TriangleList;
    pipeline_descriptor.fragment = &fragment;
    pipeline_descriptor.multisample.count = 1;

    _blur_pipeline = _device.CreateRenderPipeline(&pipeline_descriptor);
  }

  // Allocate a read-only storage buffer sized for `capacity` records. Capacity
  // is forced to at least 1 so the buffer is never zero-sized (a WGSL
  // runtime-sized array needs at least one element of backing store).
  template <typename Record> wgpu::Buffer CreateStorageBuffer(size_t capacity) {
    wgpu::BufferDescriptor descriptor;
    descriptor.size = sizeof(Record) * std::max<size_t>(1, capacity);
    descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Storage;
    return _device.CreateBuffer(&descriptor);
  }

  // Grow a layer's storage buffers to fit the record counts, rounding capacity
  // up to a power of two so steady-state frames reuse the existing buffers.
  // Returns true if any buffer was reallocated (so the bind group is rebuilt).
  bool EnsureLayerCapacity(SceneLayerBuffers &buffers, const SceneRecords &records) {
    auto next_capacity = [](size_t needed, size_t current) {
      size_t capacity = std::max<size_t>(current, 1);
      while (capacity < needed) {
        capacity *= 2;
      }
      return capacity;
    };
    bool grew = false;
    if (!buffers.panels || records.panels.size() > buffers.panel_capacity) {
      buffers.panel_capacity = next_capacity(records.panels.size(), buffers.panel_capacity);
      buffers.panels = CreateStorageBuffer<PanelUniform>(buffers.panel_capacity);
      grew = true;
    }
    if (!buffers.buttons || records.buttons.size() > buffers.button_capacity) {
      buffers.button_capacity = next_capacity(records.buttons.size(), buffers.button_capacity);
      buffers.buttons = CreateStorageBuffer<SymbolButtonUniform>(buffers.button_capacity);
      grew = true;
    }
    if (!buffers.texts || records.texts.size() > buffers.text_capacity) {
      buffers.text_capacity = next_capacity(records.texts.size(), buffers.text_capacity);
      buffers.texts = CreateStorageBuffer<TextUniform>(buffers.text_capacity);
      grew = true;
    }
    return grew;
  }

  void RebuildSceneBindGroup(SceneLayerBuffers &buffers) {
    if (!_bind_group_layout || !buffers.header || !buffers.panels || !buffers.buttons ||
        !buffers.texts || !_text_texture_view || !_text_sampler) {
      return;
    }

    std::array<wgpu::BindGroupEntry, 6> bind_group_entries{};
    bind_group_entries[0].binding = 0;
    bind_group_entries[0].buffer = buffers.header;
    bind_group_entries[0].size = sizeof(SceneHeader);

    bind_group_entries[1].binding = 1;
    bind_group_entries[1].buffer = buffers.panels;
    bind_group_entries[1].size = sizeof(PanelUniform) * buffers.panel_capacity;

    bind_group_entries[2].binding = 2;
    bind_group_entries[2].buffer = buffers.buttons;
    bind_group_entries[2].size = sizeof(SymbolButtonUniform) * buffers.button_capacity;

    bind_group_entries[3].binding = 3;
    bind_group_entries[3].buffer = buffers.texts;
    bind_group_entries[3].size = sizeof(TextUniform) * buffers.text_capacity;

    bind_group_entries[4].binding = 4;
    bind_group_entries[4].textureView = _text_texture_view;

    bind_group_entries[5].binding = 5;
    bind_group_entries[5].sampler = _text_sampler;

    wgpu::BindGroupDescriptor bind_group_descriptor;
    bind_group_descriptor.layout = _bind_group_layout;
    bind_group_descriptor.entryCount = bind_group_entries.size();
    bind_group_descriptor.entries = bind_group_entries.data();
    buffers.bind_group = _device.CreateBindGroup(&bind_group_descriptor);
  }

  void CreateSceneBindGroups() {
    RebuildSceneBindGroup(_background_buffers);
    RebuildSceneBindGroup(_foreground_buffers);
  }

  wgpu::BindGroup CreateBlurBindGroup(wgpu::Buffer uniform_buffer, wgpu::TextureView source_view) {
    if (!_blur_bind_group_layout || !uniform_buffer || !source_view || !_text_sampler) {
      return {};
    }

    std::array<wgpu::BindGroupEntry, 3> bind_group_entries{};
    bind_group_entries[0].binding = 0;
    bind_group_entries[0].buffer = uniform_buffer;
    bind_group_entries[0].offset = 0;
    bind_group_entries[0].size = sizeof(BlurUniforms);

    bind_group_entries[1].binding = 1;
    bind_group_entries[1].textureView = source_view;

    bind_group_entries[2].binding = 2;
    bind_group_entries[2].sampler = _text_sampler;

    wgpu::BindGroupDescriptor bind_group_descriptor;
    bind_group_descriptor.layout = _blur_bind_group_layout;
    bind_group_descriptor.entryCount = bind_group_entries.size();
    bind_group_descriptor.entries = bind_group_entries.data();
    return _device.CreateBindGroup(&bind_group_descriptor);
  }

  void CreateBlurBindGroups() {
    _downsample_bind_group = CreateBlurBindGroup(_downsample_uniform_buffer, _scene_texture_view);
    _horizontal_blur_bind_group =
        CreateBlurBindGroup(_horizontal_blur_uniform_buffer, _blur_texture_a_view);
    _vertical_blur_bind_group =
        CreateBlurBindGroup(_vertical_blur_uniform_buffer, _blur_texture_b_view);
  }

  void CreateEffectBindGroup() {
    if (!_effect_bind_group_layout || !_effect_uniform_buffer || !_scene_texture_view ||
        !_blur_texture_a_view || !_text_sampler) {
      return;
    }

    std::array<wgpu::BindGroupEntry, 4> bind_group_entries{};
    bind_group_entries[0].binding = 0;
    bind_group_entries[0].buffer = _effect_uniform_buffer;
    bind_group_entries[0].offset = 0;
    bind_group_entries[0].size = sizeof(EffectUniforms);

    bind_group_entries[1].binding = 1;
    bind_group_entries[1].textureView = _scene_texture_view;

    bind_group_entries[2].binding = 2;
    bind_group_entries[2].textureView = _blur_texture_a_view;

    bind_group_entries[3].binding = 3;
    bind_group_entries[3].sampler = _text_sampler;

    wgpu::BindGroupDescriptor bind_group_descriptor;
    bind_group_descriptor.layout = _effect_bind_group_layout;
    bind_group_descriptor.entryCount = bind_group_entries.size();
    bind_group_descriptor.entries = bind_group_entries.data();
    _effect_bind_group = _device.CreateBindGroup(&bind_group_descriptor);
  }

  void CreateSceneHeaderBuffer(SceneLayerBuffers &buffers) {
    wgpu::BufferDescriptor buffer_descriptor;
    buffer_descriptor.size = sizeof(SceneHeader);
    buffer_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    buffers.header = _device.CreateBuffer(&buffer_descriptor);
    // Storage buffers start at capacity 1 and grow on demand in
    // EnsureLayerCapacity; this primes them so the first bind group is valid.
    EnsureLayerCapacity(buffers, SceneRecords{});
  }

  void CreateSceneUniformBuffer() {
    CreateSceneHeaderBuffer(_background_buffers);
    CreateSceneHeaderBuffer(_foreground_buffers);

    wgpu::BufferDescriptor effect_buffer_descriptor;
    effect_buffer_descriptor.size = sizeof(EffectUniforms);
    effect_buffer_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    _effect_uniform_buffer = _device.CreateBuffer(&effect_buffer_descriptor);

    wgpu::BufferDescriptor blur_buffer_descriptor;
    blur_buffer_descriptor.size = sizeof(BlurUniforms);
    blur_buffer_descriptor.usage = wgpu::BufferUsage::CopyDst | wgpu::BufferUsage::Uniform;
    _downsample_uniform_buffer = _device.CreateBuffer(&blur_buffer_descriptor);
    _horizontal_blur_uniform_buffer = _device.CreateBuffer(&blur_buffer_descriptor);
    _vertical_blur_uniform_buffer = _device.CreateBuffer(&blur_buffer_descriptor);
  }

  void CreateSceneTexture() {
    if (!_device || _width == 0 || _height == 0 || _format == wgpu::TextureFormat::Undefined) {
      return;
    }

    wgpu::TextureDescriptor texture_descriptor;
    texture_descriptor.usage =
        wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    texture_descriptor.dimension = wgpu::TextureDimension::e2D;
    texture_descriptor.size = {
        _width,
        _height,
        1,
    };
    texture_descriptor.format = _format;
    _scene_texture = _device.CreateTexture(&texture_descriptor);
    _scene_texture_view = _scene_texture.CreateView();
    CreateBlurTextures();
    CreateBlurBindGroups();
    CreateEffectBindGroup();
  }

  void CreateBlurTextures() {
    _blur_width = std::max<uint32_t>(1, (_width + 1) / 2);
    _blur_height = std::max<uint32_t>(1, (_height + 1) / 2);

    wgpu::TextureDescriptor texture_descriptor;
    texture_descriptor.usage =
        wgpu::TextureUsage::RenderAttachment | wgpu::TextureUsage::TextureBinding;
    texture_descriptor.dimension = wgpu::TextureDimension::e2D;
    texture_descriptor.size = {
        _blur_width,
        _blur_height,
        1,
    };
    texture_descriptor.format = _format;

    _blur_texture_a = _device.CreateTexture(&texture_descriptor);
    _blur_texture_a_view = _blur_texture_a.CreateView();
    _blur_texture_b = _device.CreateTexture(&texture_descriptor);
    _blur_texture_b_view = _blur_texture_b.CreateView();
  }

  void CreateTextSampler() {
    wgpu::SamplerDescriptor sampler_descriptor;
    sampler_descriptor.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_descriptor.magFilter = wgpu::FilterMode::Linear;
    sampler_descriptor.minFilter = wgpu::FilterMode::Linear;
    sampler_descriptor.mipmapFilter = wgpu::MipmapFilterMode::Nearest;
    _text_sampler = _device.CreateSampler(&sampler_descriptor);
  }

  void CreateTextTexture(uint32_t width, uint32_t height) {
    _text_texture_width = std::max<uint32_t>(1, width);
    _text_texture_height = std::max<uint32_t>(1, height);

    wgpu::TextureDescriptor texture_descriptor;
    texture_descriptor.usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    texture_descriptor.dimension = wgpu::TextureDimension::e2D;
    texture_descriptor.size = {
        _text_texture_width,
        _text_texture_height,
        1,
    };
    texture_descriptor.format = wgpu::TextureFormat::RGBA8Unorm;
    _text_texture = _device.CreateTexture(&texture_descriptor);
    _text_texture_view = _text_texture.CreateView();
    CreateSceneBindGroups();
  }

  void UploadTextAtlas(const TextAtlas &atlas) {
    if (!_device) {
      return;
    }

    if (atlas.width != _text_texture_width || atlas.height != _text_texture_height ||
        !_text_texture) {
      CreateTextTexture(atlas.width, atlas.height);
    }
    if (!_text_texture || atlas.pixels.empty()) {
      return;
    }

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = _text_texture;
    destination.aspect = wgpu::TextureAspect::All;

    wgpu::TexelCopyBufferLayout layout;
    layout.bytesPerRow = atlas.width * 4;
    layout.rowsPerImage = atlas.height;

    wgpu::Extent3D write_size{
        atlas.width,
        atlas.height,
        1,
    };
    _device.GetQueue().WriteTexture(
        &destination, atlas.pixels.data(), atlas.pixels.size(), &layout, &write_size);
  }

  void UpdateBlurUniforms() {
    if (!_downsample_uniform_buffer || !_horizontal_blur_uniform_buffer ||
        !_vertical_blur_uniform_buffer || _width == 0 || _height == 0 || _blur_width == 0 ||
        _blur_height == 0) {
      return;
    }

    BlurUniforms downsample_uniforms = {};
    downsample_uniforms.source_size[0] = static_cast<float>(_width);
    downsample_uniforms.source_size[1] = static_cast<float>(_height);
    downsample_uniforms.source_size[2] = static_cast<float>(_blur_width);
    downsample_uniforms.source_size[3] = static_cast<float>(_blur_height);
    _device.GetQueue().WriteBuffer(
        _downsample_uniform_buffer, 0, &downsample_uniforms, sizeof(downsample_uniforms));

    float safe_scale = std::max(1.0f, _scale);
    float radius_x = std::max(1.0f, 12.0f * safe_scale * static_cast<float>(_blur_width) /
                                        std::max(1.0f, static_cast<float>(_width)));
    float radius_y = std::max(1.0f, 12.0f * safe_scale * static_cast<float>(_blur_height) /
                                        std::max(1.0f, static_cast<float>(_height)));

    BlurUniforms horizontal_uniforms = {};
    horizontal_uniforms.source_size[0] = static_cast<float>(_blur_width);
    horizontal_uniforms.source_size[1] = static_cast<float>(_blur_height);
    horizontal_uniforms.source_size[2] = static_cast<float>(_blur_width);
    horizontal_uniforms.source_size[3] = static_cast<float>(_blur_height);
    horizontal_uniforms.direction[0] = 1.0f / std::max(1.0f, static_cast<float>(_blur_width));
    horizontal_uniforms.direction[2] = radius_x;
    horizontal_uniforms.direction[3] = 1.0f;
    _device.GetQueue().WriteBuffer(
        _horizontal_blur_uniform_buffer, 0, &horizontal_uniforms, sizeof(horizontal_uniforms));

    BlurUniforms vertical_uniforms = {};
    vertical_uniforms.source_size[0] = static_cast<float>(_blur_width);
    vertical_uniforms.source_size[1] = static_cast<float>(_blur_height);
    vertical_uniforms.source_size[2] = static_cast<float>(_blur_width);
    vertical_uniforms.source_size[3] = static_cast<float>(_blur_height);
    vertical_uniforms.direction[1] = 1.0f / std::max(1.0f, static_cast<float>(_blur_height));
    vertical_uniforms.direction[2] = radius_y;
    vertical_uniforms.direction[3] = 1.0f;
    _device.GetQueue().WriteBuffer(
        _vertical_blur_uniform_buffer, 0, &vertical_uniforms, sizeof(vertical_uniforms));
  }

  // Fill a layer's header + records, grow its storage buffers if needed
  // (rebuilding the bind group on growth), then upload header and records.
  void UploadSceneLayer(SceneLayerBuffers &buffers, const SceneDrawLayer &layer,
      const TextAtlas &text_atlas, size_t text_offset, size_t symbol_offset,
      LayoutRect viewport_clip) {
    SceneHeader header = {};
    SceneRecords records;
    FillSceneRecords(header, records, layer, text_atlas, text_offset, symbol_offset,
        static_cast<float>(_width), static_cast<float>(_height), _scale, viewport_clip);

    if (EnsureLayerCapacity(buffers, records)) {
      RebuildSceneBindGroup(buffers);
    }
    buffers.panel_count = static_cast<uint32_t>(records.panels.size());
    buffers.button_count = static_cast<uint32_t>(records.buttons.size());
    buffers.text_count = static_cast<uint32_t>(records.texts.size());

    _device.GetQueue().WriteBuffer(buffers.header, 0, &header, sizeof(header));
    if (!records.panels.empty()) {
      _device.GetQueue().WriteBuffer(
          buffers.panels, 0, records.panels.data(), records.panels.size() * sizeof(PanelUniform));
    }
    if (!records.buttons.empty()) {
      _device.GetQueue().WriteBuffer(buffers.buttons, 0, records.buttons.data(),
          records.buttons.size() * sizeof(SymbolButtonUniform));
    }
    if (!records.texts.empty()) {
      _device.GetQueue().WriteBuffer(
          buffers.texts, 0, records.texts.data(), records.texts.size() * sizeof(TextUniform));
    }
  }

  void UpdateSceneUniforms() {
    if (!_background_buffers.header || !_foreground_buffers.header || !_effect_uniform_buffer) {
      return;
    }

    SceneLayout scene = LayoutScene(
        MeasureText, _root_view, _layout_size.width, _layout_size.height, _layout_context);
    _hit_targets = scene.hit_targets;
    _scroll_targets = scene.scroll_targets;
    _text_input_targets = scene.text_input_targets;

    std::vector<TextLayout> atlas_texts;
    atlas_texts.reserve(scene.background.texts.size() + scene.foreground.texts.size());
    AppendTextLayouts(atlas_texts, scene.background.texts);
    AppendTextLayouts(atlas_texts, scene.foreground.texts);

    std::vector<SymbolButtonLayout> atlas_buttons;
    atlas_buttons.reserve(scene.background.buttons.size() + scene.foreground.buttons.size());
    AppendButtonLayouts(atlas_buttons, scene.background.buttons);
    AppendButtonLayouts(atlas_buttons, scene.foreground.buttons);

    TextAtlasCacheKey text_atlas_key = MakeTextAtlasCacheKey(atlas_texts, atlas_buttons, _scale);
    if (!_has_text_atlas_cache || !(text_atlas_key == _text_atlas_cache_key)) {
      _text_atlas_cache = BuildTextAtlas(atlas_texts, atlas_buttons, _scale);
      UploadTextAtlas(_text_atlas_cache);
      _text_atlas_cache_key = text_atlas_key;
      _has_text_atlas_cache = true;
    }
    TextAtlas text_atlas = RetargetTextAtlasEntries(_text_atlas_cache, atlas_texts, atlas_buttons);

    LayoutRect viewport_clip{
        0.0f,
        0.0f,
        _layout_size.width,
        _layout_size.height,
    };

    UploadSceneLayer(_background_buffers, scene.background, text_atlas, 0, 0, viewport_clip);
    UploadSceneLayer(_foreground_buffers, scene.foreground, text_atlas,
        scene.background.texts.size(), scene.background.buttons.size(), viewport_clip);

    EffectUniforms effect_uniforms = {};
    effect_uniforms.viewport[0] = static_cast<float>(_width);
    effect_uniforms.viewport[1] = static_cast<float>(_height);
    effect_uniforms.viewport[3] = std::max(1.0f, _scale);
    _has_effect_panels = false;
    _blur_scissor.reset();
    if (std::optional<LayoutRect> effect_bounds = EffectBounds(scene.effects)) {
      constexpr float blur_padding = 48.0f;
      PixelRect effect_scissor =
          ScaleToPixelRect(*effect_bounds, _scale, _width, _height, blur_padding);
      _blur_scissor = ScalePixelRect(effect_scissor, _width, _height, _blur_width, _blur_height);
      _has_effect_panels = _blur_scissor.has_value() && HasArea(*_blur_scissor);
    }
    effect_uniforms.counts[0] =
        _has_effect_panels ? static_cast<float>(scene.effects.size()) : 0.0f;
    for (size_t index = 0; index < scene.effects.size(); ++index) {
      effect_uniforms.effects[index] = MakeEffectPanel(scene.effects[index], _scale, viewport_clip);
    }
    _device.GetQueue().WriteBuffer(
        _effect_uniform_buffer, 0, &effect_uniforms, sizeof(effect_uniforms));
  }

  wgpu::Instance _instance;
  wgpu::Surface _surface;
  wgpu::Adapter _adapter;
  wgpu::Device _device;
  SceneLayerBuffers _background_buffers;
  SceneLayerBuffers _foreground_buffers;
  wgpu::Buffer _effect_uniform_buffer;
  wgpu::Buffer _downsample_uniform_buffer;
  wgpu::Buffer _horizontal_blur_uniform_buffer;
  wgpu::Buffer _vertical_blur_uniform_buffer;
  wgpu::BindGroupLayout _bind_group_layout;
  wgpu::BindGroupLayout _effect_bind_group_layout;
  wgpu::BindGroupLayout _blur_bind_group_layout;
  wgpu::BindGroup _effect_bind_group;
  wgpu::BindGroup _downsample_bind_group;
  wgpu::BindGroup _horizontal_blur_bind_group;
  wgpu::BindGroup _vertical_blur_bind_group;
  wgpu::Texture _scene_texture;
  wgpu::TextureView _scene_texture_view;
  wgpu::Texture _blur_texture_a;
  wgpu::TextureView _blur_texture_a_view;
  wgpu::Texture _blur_texture_b;
  wgpu::TextureView _blur_texture_b_view;
  wgpu::Texture _text_texture;
  wgpu::TextureView _text_texture_view;
  wgpu::Sampler _text_sampler;
  wgpu::TextureFormat _format = wgpu::TextureFormat::Undefined;
  wgpu::RenderPipeline _panel_pipeline;
  wgpu::RenderPipeline _button_pipeline;
  wgpu::RenderPipeline _text_pipeline;
  wgpu::RenderPipeline _effect_pipeline;
  wgpu::RenderPipeline _blur_pipeline;
  phenotype::ui::View _root_view;
  phenotype::ui::Size _layout_size;
  LayoutContext _layout_context;
  std::vector<HitTargetLayout> _hit_targets;
  std::vector<ScrollTargetLayout> _scroll_targets;
  std::vector<TextInputTargetLayout> _text_input_targets;
  TextAtlasCacheKey _text_atlas_cache_key;
  TextAtlas _text_atlas_cache;
  std::optional<PixelRect> _blur_scissor;
  float _scale = 1.0f;
  bool _has_text_atlas_cache = false;
  bool _has_effect_panels = false;
  uint32_t _width = 0;
  uint32_t _height = 0;
  uint32_t _blur_width = 0;
  uint32_t _blur_height = 0;
  uint32_t _text_texture_width = 0;
  uint32_t _text_texture_height = 0;
};

} // namespace

@protocol PhenotypeMetalViewDelegate
- (void)metalViewNeedsRender:(NSView *)view;
- (BOOL)metalView:(NSView *)view mouseDownAt:(NSPoint)location;
- (BOOL)metalView:(NSView *)view scrollAt:(NSPoint)location deltaY:(CGFloat)deltaY;
- (BOOL)metalView:(NSView *)view textEdit:(const phenotype::ui::TextEdit &)edit;
- (BOOL)metalViewHasTextInput:(NSView *)view;
// The focused field's currently-selected committed text (empty if none), for
// clipboard copy/cut.
- (NSString *)metalViewSelectedText:(NSView *)view;
// The focused field's caret rect in the view's coordinate space, for the IME
// candidate window. Returns NSZeroRect when nothing is focused.
- (NSRect)metalViewCaretRect:(NSView *)view;
@end

@interface PhenotypeMetalView : NSView <NSTextInputClient> {
  id<PhenotypeMetalViewDelegate> _renderDelegate;
  NSString *_markedText;
}
@property(nonatomic, assign) id<PhenotypeMetalViewDelegate> renderDelegate;
@end

@implementation PhenotypeMetalView

@synthesize renderDelegate = _renderDelegate;

- (void)requestRender {
  [_renderDelegate metalViewNeedsRender:self];
}

- (BOOL)acceptsFirstResponder {
  return YES;
}

- (BOOL)acceptsFirstMouse:(NSEvent *)event {
  (void)event;
  return YES;
}

- (BOOL)mouseDownCanMoveWindow {
  return NO;
}

- (void)mouseDown:(NSEvent *)event {
  [[self window] makeFirstResponder:self];
  NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
  if (![_renderDelegate metalView:self mouseDownAt:location]) {
    [[self window] performWindowDragWithEvent:event];
  }
}

- (void)scrollWheel:(NSEvent *)event {
  NSPoint location = [self convertPoint:[event locationInWindow] fromView:nil];
  if (![_renderDelegate metalView:self scrollAt:location deltaY:[event scrollingDeltaY]]) {
    [super scrollWheel:event];
  }
}

- (void)keyDown:(NSEvent *)event {
  // Only consume keys when a text field is focused; otherwise pass through so
  // system shortcuts and the responder chain still work.
  if (![_renderDelegate metalViewHasTextInput:self]) {
    [super keyDown:event];
    return;
  }
  // Hand the event straight to the input context so the active IME composes
  // multi-byte input (Hangul/Japanese/Chinese): it calls back into our
  // NSTextInputClient methods (insertText: for committed text, setMarkedText:
  // while composing) and doCommandBySelector: for editing keys. Unlike
  // interpretKeyEvents: (NSResponder path), handleEvent: makes this view the
  // current input client immediately, so the FIRST key after switching to a
  // composing IME starts a composition instead of being committed outright.
  if (![[self inputContext] handleEvent:event]) {
    [super keyDown:event];
  }
}

// --- editing commands routed from interpretKeyEvents: via doCommandBySelector:
- (void)doCommandBySelector:(SEL)selector {
  using Edit = phenotype::ui::TextEdit;
  std::optional<Edit> edit;
  if (selector == @selector(deleteBackward:)) {
    edit = Edit{Edit::Kind::delete_backward, {}, false, 0};
  } else if (selector == @selector(deleteForward:)) {
    edit = Edit{Edit::Kind::delete_forward, {}, false, 0};
  } else if (selector == @selector(moveLeft:)) {
    edit = Edit{Edit::Kind::move_left, {}, false, 0};
  } else if (selector == @selector(moveRight:)) {
    edit = Edit{Edit::Kind::move_right, {}, false, 0};
  } else if (selector == @selector(moveLeftAndModifySelection:)) {
    edit = Edit{Edit::Kind::move_left, {}, true, 0};
  } else if (selector == @selector(moveRightAndModifySelection:)) {
    edit = Edit{Edit::Kind::move_right, {}, true, 0};
  } else if (selector == @selector(moveToBeginningOfLine:) ||
             selector == @selector(moveToLeftEndOfLine:) ||
             selector == @selector(scrollToBeginningOfDocument:)) {
    edit = Edit{Edit::Kind::move_home, {}, false, 0};
  } else if (selector == @selector(moveToEndOfLine:) ||
             selector == @selector(moveToRightEndOfLine:) ||
             selector == @selector(scrollToEndOfDocument:)) {
    edit = Edit{Edit::Kind::move_end, {}, false, 0};
  } else if (selector == @selector(selectAll:)) {
    edit = Edit{Edit::Kind::select_all, {}, false, 0};
  }
  if (edit) {
    [_renderDelegate metalView:self textEdit:*edit];
  }
  // Other selectors (insertNewline:, cancelOperation:, etc.) are intentionally
  // ignored for this single-line field.
}

// Commit any in-progress IME composition into the field, so a following command
// (select-all, copy, etc.) operates on the whole text including what was being
// composed. The active input context is also told to finish.
- (void)commitMarkedText {
  if (_markedText == nil) {
    return;
  }
  using Edit = phenotype::ui::TextEdit;
  std::string committed([_markedText UTF8String]);
  _markedText = nil;
  [[self inputContext] discardMarkedText];
  if (!committed.empty()) {
    [_renderDelegate metalView:self textEdit:Edit{Edit::Kind::insert, committed, false, 0}];
  } else {
    [_renderDelegate metalView:self textEdit:Edit{Edit::Kind::unmark, "", false, 0}];
  }
}

- (BOOL)performKeyEquivalent:(NSEvent *)event {
  // Clipboard + select-all shortcuts for a focused field. Match on the physical
  // key code, NOT the typed character: under a non-Latin IME (e.g. Hangul) the
  // character for the A key is "ㅁ", so a character compare would miss Cmd+A.
  // Virtual key codes are layout-independent: A=0, X=7, C=8, V=9.
  NSEventModifierFlags const flags = [event modifierFlags];
  bool const command_only = (flags & NSEventModifierFlagCommand) != 0 &&
                            (flags & (NSEventModifierFlagControl | NSEventModifierFlagOption)) == 0;
  if ([_renderDelegate metalViewHasTextInput:self] && command_only) {
    using Edit = phenotype::ui::TextEdit;
    // Finish any composition first so the command sees the committed text.
    [self commitMarkedText];
    switch ([event keyCode]) {
    case 0: // A — select all
      return [_renderDelegate metalView:self textEdit:Edit{Edit::Kind::select_all, {}, false, 0}];
    case 8: // C — copy
    case 7: // X — cut
    {
      NSString *selected = [_renderDelegate metalViewSelectedText:self];
      if (selected.length > 0) {
        NSPasteboard *pasteboard = [NSPasteboard generalPasteboard];
        [pasteboard clearContents];
        [pasteboard setString:selected forType:NSPasteboardTypeString];
        if ([event keyCode] == 7) {
          // Cut: replace the selection with empty text.
          [_renderDelegate metalView:self textEdit:Edit{Edit::Kind::insert, "", false, 0}];
        }
      }
      return YES;
    }
    case 9: // V — paste
    {
      NSString *pasted = [[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
      if (pasted.length > 0) {
        // Collapse newlines to spaces for the single-line field.
        NSString *flat =
            [[pasted componentsSeparatedByCharactersInSet:[NSCharacterSet newlineCharacterSet]]
                componentsJoinedByString:@" "];
        [_renderDelegate
            metalView:self
             textEdit:Edit{Edit::Kind::insert, std::string([flat UTF8String]), false, 0}];
      }
      return YES;
    }
    default:
      break;
    }
  }
  return [super performKeyEquivalent:event];
}

// --- NSTextInputClient: the bridge to the active input method (IME) ----------

- (void)insertText:(id)string replacementRange:(NSRange)replacementRange {
  (void)replacementRange;
  NSString *text = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
  using Edit = phenotype::ui::TextEdit;
  // Committed text ends any composition and inserts the final characters.
  if (_markedText != nil) {
    _markedText = nil;
  }
  if (text.length > 0) {
    [_renderDelegate metalView:self
                      textEdit:Edit{Edit::Kind::insert, std::string([text UTF8String]), false, 0}];
  }
}

- (void)setMarkedText:(id)string
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  (void)replacementRange;
  NSString *text = [string isKindOfClass:[NSAttributedString class]] ? [string string] : string;
  _markedText = text.length > 0 ? [text copy] : nil;
  using Edit = phenotype::ui::TextEdit;
  if (_markedText == nil) {
    [_renderDelegate metalView:self textEdit:Edit{Edit::Kind::unmark, "", false, 0}];
    return;
  }
  // The composition caret: selectedRange is in UTF-16 units within the marked
  // string; convert to a byte offset for the component.
  NSString *prefix =
      selectedRange.location <= text.length ? [text substringToIndex:selectedRange.location] : text;
  std::size_t marked_caret = std::string([prefix UTF8String]).size();
  [_renderDelegate
      metalView:self
       textEdit:Edit{Edit::Kind::set_marked, std::string([text UTF8String]), false, marked_caret}];
}

- (void)unmarkText {
  _markedText = nil;
  using Edit = phenotype::ui::TextEdit;
  [_renderDelegate metalView:self textEdit:Edit{Edit::Kind::unmark, "", false, 0}];
}

- (BOOL)hasMarkedText {
  return _markedText != nil;
}

- (NSRange)markedRange {
  if (_markedText == nil) {
    return NSMakeRange(NSNotFound, 0);
  }
  return NSMakeRange(0, _markedText.length);
}

- (NSRange)selectedRange {
  // The component owns the real selection; the IME only needs a coherent
  // non-NSNotFound value. While composing the caret sits at the end of the
  // marked text; otherwise report an empty range at the origin.
  return NSMakeRange(_markedText != nil ? _markedText.length : 0, 0);
}

- (NSArray<NSAttributedStringKey> *)validAttributesForMarkedText {
  return @[];
}

- (NSAttributedString *)attributedSubstringForProposedRange:(NSRange)range
                                                actualRange:(NSRangePointer)actualRange {
  (void)range;
  // Return an empty (non-nil) attributed string and a zero-length actual range.
  // Some input methods probe this before starting a composition and treat a nil
  // result as "no text client", committing the first key instead of composing.
  if (actualRange != nullptr) {
    *actualRange = NSMakeRange(0, 0);
  }
  return [[NSAttributedString alloc] initWithString:@""];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range actualRange:(NSRangePointer)actualRange {
  (void)range;
  (void)actualRange;
  // Place the IME candidate window at the caret: convert the field's caret rect
  // (view space, top-left origin) to screen coordinates (bottom-left origin).
  NSRect caret = [_renderDelegate metalViewCaretRect:self];
  if (NSEqualRects(caret, NSZeroRect)) {
    return NSZeroRect;
  }
  NSRect flipped =
      NSMakeRect(caret.origin.x, [self bounds].size.height - caret.origin.y - caret.size.height,
          caret.size.width, caret.size.height);
  NSRect window_rect = [self convertRect:flipped toView:nil];
  return [[self window] convertRectToScreen:window_rect];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)point {
  (void)point;
  return NSNotFound;
}

- (void)setFrameSize:(NSSize)newSize {
  NSSize old_size = [self frame].size;
  [super setFrameSize:newSize];
  if (!NSEqualSizes(old_size, newSize)) {
    [self requestRender];
  }
}

- (void)setBoundsSize:(NSSize)newSize {
  NSSize old_size = [self bounds].size;
  [super setBoundsSize:newSize];
  if (!NSEqualSizes(old_size, newSize)) {
    [self requestRender];
  }
}

- (void)viewWillStartLiveResize {
  [super viewWillStartLiveResize];
  [self requestRender];
}

- (void)viewDidEndLiveResize {
  [super viewDidEndLiveResize];
  [self requestRender];
}

@end

@interface AppDelegate
    : NSObject <NSApplicationDelegate, NSWindowDelegate, PhenotypeMetalViewDelegate> {
  NSWindow *_window;
  NSTimer *_render_timer;
  NSView *_metal_view;
  CAMetalLayer *_metal_layer;
  std::unique_ptr<DawnButtonRenderer> _renderer;
  phenotype::macos::window::Spec _spec;
  std::array<NSRect, 3> _window_control_base_frames;
  std::array<bool, 3> _window_control_has_base_frame;
  LayoutWindowControls _stable_window_controls;
  bool _has_window_control_base_frames;
  bool _is_rendering;
  bool _render_requested_after_current;
  bool _needs_render;
}
- (instancetype)initWithSpec:(phenotype::macos::window::Spec)spec;
- (LayoutContext)buildLayoutContext;
- (void)applyWindowControlOffset;
- (void)refreshRootView;
- (void)requestFrame;
- (void)renderFrameIfNeeded;
- (phenotype::ui::Size)layoutPointForLocation:(NSPoint)location;
@end

@implementation AppDelegate

- (instancetype)initWithSpec:(phenotype::macos::window::Spec)spec {
  self = [super init];
  if (self) {
    _spec = std::move(spec);
    _window_control_base_frames.fill(NSZeroRect);
    _window_control_has_base_frame.fill(false);
    _stable_window_controls = {};
    _has_window_control_base_frames = false;
    _is_rendering = false;
    _render_requested_after_current = false;
    _needs_render = false;
  }
  return self;
}

- (void)applicationDidFinishLaunching:(NSNotification *)notification {
  (void)notification;

  CGFloat window_width = std::max<CGFloat>(1.0, _spec.options.size.width);
  CGFloat window_height = std::max<CGFloat>(1.0, _spec.options.size.height);
  NSRect content_rect = NSMakeRect(0.0, 0.0, window_width, window_height);
  bool hides_title_bar = _spec.options.title_bar == phenotype::macos::window::TitleBarStyle::hidden;
  NSWindowStyleMask style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                                 NSWindowStyleMaskMiniaturizable | NSWindowStyleMaskResizable;
  if (hides_title_bar) {
    style_mask |= NSWindowStyleMaskFullSizeContentView;
  }

  _window = [[NSWindow alloc] initWithContentRect:content_rect
                                        styleMask:style_mask
                                          backing:NSBackingStoreBuffered
                                            defer:NO];
  [_window setDelegate:self];
  [_window setTitle:[NSString stringWithUTF8String:_spec.options.title.c_str()]];
  ApplyTitleBarStyle(_window, _spec.options.title_bar);

  using BackgroundKind = phenotype::macos::window::Background::Kind;
  BackgroundKind background_kind = _spec.options.background.kind;
  bool uses_blur = background_kind == BackgroundKind::blurred;
  bool uses_solid = background_kind == BackgroundKind::solid;

  NSColor *solid_background_color = nil;
  if (uses_solid) {
    const phenotype::ui::Color &fill = _spec.options.background.color;
    // Clamp alpha to opaque: a solid background backs an opaque window, so any
    // requested translucency would only reveal the desktop, not blur it.
    solid_background_color =
        [NSColor colorWithSRGBRed:fill.red green:fill.green blue:fill.blue alpha:1.0];
  }

  if (uses_blur) {
    [_window setOpaque:NO];
    [_window setBackgroundColor:[NSColor clearColor]];
  } else if (uses_solid) {
    [_window setOpaque:YES];
    [_window setBackgroundColor:solid_background_color];
  } else {
    [_window setOpaque:YES];
    [_window setBackgroundColor:[NSColor windowBackgroundColor]];
  }

  NSRect visible_frame = InitialWindowVisibleFrame(window_width, window_height);
  [_window setFrameOrigin:NSMakePoint(NSMidX(visible_frame) - (window_width / 2.0),
                              NSMidY(visible_frame) - (window_height / 2.0))];

  NSView *content_view = nil;
  if (uses_blur) {
    NSVisualEffectView *visual_effect_view =
        [[NSVisualEffectView alloc] initWithFrame:content_rect];
    [visual_effect_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [visual_effect_view setMaterial:ToNativeVisualMaterial(_spec.options.background.blur.material)];
    [visual_effect_view setBlendingMode:NSVisualEffectBlendingModeBehindWindow];
    [visual_effect_view setState:NSVisualEffectStateActive];
    [visual_effect_view setEmphasized:YES];
    [visual_effect_view setAlphaValue:_spec.options.background.blur.opacity];
    content_view = visual_effect_view;
  } else if (uses_solid) {
    // An opaque view whose layer carries the fill. The Metal layer above stays
    // transparent and clears to nothing, so this solid layer is what shows
    // through wherever the scene does not paint.
    content_view = [[NSView alloc] initWithFrame:content_rect];
    [content_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [content_view setWantsLayer:YES];
    [[content_view layer] setOpaque:YES];
    [[content_view layer] setBackgroundColor:[solid_background_color CGColor]];
  } else {
    content_view = [[NSView alloc] initWithFrame:content_rect];
    [content_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [content_view setWantsLayer:YES];
    [[content_view layer] setOpaque:NO];
    [[content_view layer] setBackgroundColor:[[NSColor clearColor] CGColor]];
  }

  _metal_view = [[PhenotypeMetalView alloc] initWithFrame:content_rect];
  [(PhenotypeMetalView *)_metal_view setRenderDelegate:self];
  [_metal_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
  [_metal_view setWantsLayer:YES];

  _metal_layer = [CAMetalLayer layer];
  [_metal_layer setOpaque:NO];
  [_metal_layer setBackgroundColor:[[NSColor clearColor] CGColor]];
  [_metal_layer setContentsGravity:kCAGravityTopLeft];
  [_metal_layer setFrame:NSRectToCGRect([_metal_view bounds])];
  [_metal_layer setContentsScale:[_window backingScaleFactor]];
  [_metal_layer setDrawableSize:CGSizeMake(LogicalPixel(window_width, [_window backingScaleFactor]),
                                    LogicalPixel(window_height, [_window backingScaleFactor]))];
  [_metal_view setLayer:_metal_layer];
  [content_view addSubview:_metal_view];
  [_metal_view release];

  [_window setContentView:content_view];
  [content_view release];
  ApplyTitleBarStyle(_window, _spec.options.title_bar);

  [_window makeKeyAndOrderFront:nil];
  [NSApp activate];
  [self applyWindowControlOffset];

  CGFloat scale = [_window backingScaleFactor];
  NSSize bounds = [_metal_view bounds].size;
  phenotype::ui::Size layout_size{
      static_cast<float>(bounds.width),
      static_cast<float>(bounds.height),
  };
  LayoutContext layout_context = [self buildLayoutContext];

  phenotype::ui::View root_view = _spec.content ? _spec.content() : phenotype::ui::empty();
  _renderer = std::make_unique<DawnButtonRenderer>();
  if (!_renderer->Initialize(_metal_layer, PixelSize(bounds.width, scale),
          PixelSize(bounds.height, scale), static_cast<float>(scale), layout_size, layout_context,
          std::move(root_view))) {
    _renderer.reset();
    [NSApp terminate:nil];
    return;
  }

  _render_timer = [NSTimer timerWithTimeInterval:(1.0 / 60.0)
                                          target:self
                                        selector:@selector(renderFrame:)
                                        userInfo:nil
                                         repeats:YES];
  [[NSRunLoop mainRunLoop] addTimer:_render_timer forMode:NSRunLoopCommonModes];
  [self renderNow];
}

- (void)requestFrame {
  _needs_render = true;
}

- (phenotype::ui::Size)layoutPointForLocation:(NSPoint)location {
  NSSize bounds = [_metal_view bounds].size;
  return {
      static_cast<float>(location.x),
      static_cast<float>(bounds.height - location.y),
  };
}

- (void)renderFrameIfNeeded {
  if (!_needs_render) {
    return;
  }

  _needs_render = false;
  [self renderNow];
}

- (void)renderNow {
  if (!_window || !_metal_view || !_metal_layer) {
    return;
  }
  if (_is_rendering) {
    _render_requested_after_current = true;
    return;
  }
  _is_rendering = true;
  do {
    _render_requested_after_current = false;
    [self renderOnce];
  } while (_render_requested_after_current);
  _needs_render = false;
  _is_rendering = false;
}

- (void)refreshRootView {
  if (!_renderer || !_spec.content) {
    return;
  }
  _renderer->UpdateRootView(_spec.content());
}

- (void)renderOnce {
  CGFloat scale = [_window backingScaleFactor];
  NSRect bounds_rect = [_metal_view bounds];
  NSSize bounds = bounds_rect.size;
  if (bounds.width <= 0.0 || bounds.height <= 0.0) {
    return;
  }
  CGSize drawable_size = CGSizeMake(bounds.width * scale, bounds.height * scale);
  [_metal_layer setFrame:NSRectToCGRect(bounds_rect)];
  [_metal_layer setContentsScale:scale];
  [_metal_layer setDrawableSize:drawable_size];

  if (_renderer) {
    phenotype::ui::Size layout_size{
        static_cast<float>(bounds.width),
        static_cast<float>(bounds.height),
    };
    LayoutContext layout_context = [self buildLayoutContext];
    _renderer->Resize(PixelSize(bounds.width, scale), PixelSize(bounds.height, scale),
        static_cast<float>(scale), layout_size, layout_context);
    _renderer->Render();
  }
}

- (LayoutContext)buildLayoutContext {
  LayoutContext context;
  if (_stable_window_controls.has_leading_controls) {
    context.window_controls = _stable_window_controls;
    return context;
  }

  context = BuildLayoutContext(_window, [_window contentView]);
  if (context.window_controls.has_leading_controls) {
    _stable_window_controls = context.window_controls;
  }
  return context;
}

- (void)applyWindowControlOffset {
  if (!_has_window_control_base_frames) {
    _has_window_control_base_frames = CaptureWindowControlFrames(
        _window, _window_control_base_frames, _window_control_has_base_frame);
  }
  ApplyWindowControlVerticalOffset(_window, _window_control_base_frames,
      _window_control_has_base_frame, _spec.options.window_controls.vertical_offset);
}

- (void)windowDidResize:(NSNotification *)notification {
  if ([notification object] != _window) {
    return;
  }
  [self applyWindowControlOffset];
  [self renderNow];
}

- (void)windowDidEndLiveResize:(NSNotification *)notification {
  if ([notification object] != _window) {
    return;
  }
  [self applyWindowControlOffset];
  [self renderNow];
}

- (void)renderFrame:(NSTimer *)timer {
  (void)timer;
  // While an animation is in flight, rebuild each tick so animate_* re-samples
  // the clock and advances; the runtime clears needs_tick once everything
  // settles, so this naturally stops re-arming and the app goes idle.
  if (_spec.wants_animation_frame && _spec.wants_animation_frame()) {
    [self refreshRootView];
    [self requestFrame];
  }
  [self renderFrameIfNeeded];
}

- (void)metalViewNeedsRender:(NSView *)view {
  if (view == _metal_view) {
    [self requestFrame];
  }
}

- (BOOL)metalView:(NSView *)view mouseDownAt:(NSPoint)location {
  if (view != _metal_view || !_renderer) {
    return NO;
  }
  phenotype::ui::Size layout_point = [self layoutPointForLocation:location];
  if (_renderer->ActivateAt(layout_point)) {
    [self refreshRootView];
    [self renderNow];
    return YES;
  }
  return NO;
}

- (BOOL)metalView:(NSView *)view scrollAt:(NSPoint)location deltaY:(CGFloat)deltaY {
  if (view != _metal_view || !_renderer) {
    return NO;
  }

  phenotype::ui::Size layout_point = [self layoutPointForLocation:location];
  if (!_renderer->HasScrollTargetAt(layout_point)) {
    return NO;
  }

  if (_renderer->ScrollAt(layout_point, static_cast<float>(deltaY))) {
    [self refreshRootView];
    [self renderNow];
  }
  return YES;
}

- (BOOL)metalViewHasTextInput:(NSView *)view {
  if (view != _metal_view || !_renderer) {
    return NO;
  }
  return _renderer->HasTextInput() ? YES : NO;
}

- (BOOL)metalView:(NSView *)view textEdit:(const phenotype::ui::TextEdit &)edit {
  if (view != _metal_view || !_renderer) {
    return NO;
  }
  if (_renderer->DispatchTextEdit(edit)) {
    [self refreshRootView];
    [self renderNow];
    return YES;
  }
  return NO;
}

- (NSString *)metalViewSelectedText:(NSView *)view {
  if (view != _metal_view || !_renderer) {
    return @"";
  }
  std::string text;
  size_t begin = 0;
  size_t end = 0;
  if (!_renderer->FocusedSelection(text, begin, end) || begin >= end || end > text.size()) {
    return @"";
  }
  std::string selected = text.substr(begin, end - begin);
  return [NSString stringWithUTF8String:selected.c_str()] ?: @"";
}

- (NSRect)metalViewCaretRect:(NSView *)view {
  if (view != _metal_view || !_renderer) {
    return NSZeroRect;
  }
  LayoutRect caret{};
  if (!_renderer->FocusedCaretRect(caret)) {
    return NSZeroRect;
  }
  // Layout space (top-left origin, points) — the metal view flips to AppKit's
  // bottom-left origin when reporting to the IME.
  return NSMakeRect(caret.x, caret.y, std::max(caret.width, 1.0f), caret.height);
}

- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication *)sender {
  (void)sender;
  return YES;
}

- (void)dealloc {
  [_render_timer invalidate];
  [_window setDelegate:nil];
  [(PhenotypeMetalView *)_metal_view setRenderDelegate:nil];
  _renderer.reset();
  [_window release];
  [super dealloc];
}

@end

extern "C" int phenotype_macos_app_run(
    int argc, char *argv[], phenotype::macos::window::Spec *spec) {
  (void)argc;
  (void)argv;

  @autoreleasepool {
    NSApplication *application = [NSApplication sharedApplication];
    AppDelegate *delegate = [[AppDelegate alloc] initWithSpec:std::move(*spec)];

    [application setActivationPolicy:NSApplicationActivationPolicyRegular];
    [application setDelegate:delegate];
    [application run];

    [delegate release];
  }

  return 0;
}

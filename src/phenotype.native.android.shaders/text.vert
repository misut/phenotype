#version 450

// Text pipeline — direct port of phenotype.native.macos.cppm's vs_text
// (MSL) and phenotype.native.windows.cppm's vs_text (HLSL). Shares the
// TextInstance layout (stride 48) with the desktop backends so atlas
// UVs and color modulation match byte-for-byte.

layout(std140, binding = 0) uniform Uniforms {
    vec2 viewport;    // swapchain extent in pixels
    vec2 _pad;        // std140 alignment
};

struct TextInstance {
    vec4 rect;        // x, y, w, h (pixels, top-left origin)
    vec4 uv_rect;     // u, v, uw, vh (normalized atlas coords)
    vec4 color;       // linear RGBA 0..1
};

layout(std430, binding = 1) readonly buffer TextInstances {
    TextInstance instances[];
};

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_uv;

const vec2 CORNERS[6] = vec2[6](
    vec2(0, 0), vec2(1, 0), vec2(0, 1),
    vec2(1, 0), vec2(1, 1), vec2(0, 1)
);

void main() {
    vec2 c = CORNERS[gl_VertexIndex];
    TextInstance inst = instances[gl_InstanceIndex];

    float px = inst.rect.x + c.x * inst.rect.z;
    float py = inst.rect.y + c.y * inst.rect.w;

    // Vulkan NDC: +Y points down, so the same linear mapping we use in
    // color.vert carries over verbatim.
    float cx = (px / viewport.x) * 2.0 - 1.0;
    float cy = (py / viewport.y) * 2.0 - 1.0;

    gl_Position = vec4(cx, cy, 0.0, 1.0);
    v_color = inst.color;
    v_uv = inst.uv_rect.xy + c * inst.uv_rect.zw;
}

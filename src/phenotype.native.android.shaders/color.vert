#version 450

// Color pipeline — direct port of phenotype.native.macos.cppm's vs_color
// (MSL) and phenotype.native.windows.cppm's vs_color (HLSL). Shares the
// ColorInstance layout (stride 48) and params encoding with the desktop
// backends so command buffers serialize identically on every platform.

layout(std140, binding = 0) uniform Uniforms {
    vec2 viewport;     // swapchain extent in pixels
    vec2 _pad;         // std140 alignment; unused
};

struct ColorInstance {
    vec4 rect;         // x, y, w, h (pixels, top-left origin)
    vec4 color;        // linear RGBA 0..1
    vec4 params;       // radius, border_w, draw_type, unused
};

layout(std430, binding = 1) readonly buffer Instances {
    ColorInstance instances[];
};

layout(location = 0) out vec4 v_color;
layout(location = 1) out vec2 v_local_pos;
layout(location = 2) out vec2 v_rect_size;
layout(location = 3) out vec4 v_params;

// Two triangles per quad, corners in pixel-local space (0..1).
const vec2 CORNERS[6] = vec2[6](
    vec2(0, 0), vec2(1, 0), vec2(0, 1),
    vec2(1, 0), vec2(1, 1), vec2(0, 1)
);

void main() {
    vec2 c = CORNERS[gl_VertexIndex];
    ColorInstance inst = instances[gl_InstanceIndex];

    float px = inst.rect.x + c.x * inst.rect.z;
    float py = inst.rect.y + c.y * inst.rect.w;

    float cx = (px / viewport.x) * 2.0 - 1.0;
    // Vulkan NDC has +Y pointing down (opposite of Metal/GL). Straight
    // linear map from pixel Y (top-left origin) to clip Y (top-to-bottom).
    float cy = (py / viewport.y) * 2.0 - 1.0;

    gl_Position = vec4(cx, cy, 0.0, 1.0);
    v_color     = inst.color;
    v_local_pos = c * inst.rect.zw;
    v_rect_size = inst.rect.zw;
    v_params    = inst.params;
}

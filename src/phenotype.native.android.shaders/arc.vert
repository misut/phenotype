#version 450

// Arc pipeline — direct port of phenotype.native.macos.cppm's vs_arc
// (MSL). Shares the ArcInstance layout (stride 48) with the desktop
// backend so cmd-buffer serialisation stays identical across
// platforms.

layout(std140, binding = 0) uniform Uniforms {
    vec2 viewport;     // swapchain extent in pixels
    vec2 _pad;         // std140 alignment; unused
};

struct ArcInstance {
    vec4 center_radius_thickness;  // cx, cy, radius, thickness
    vec4 angles;                    // start, end, _, _
    vec4 color;                     // linear RGBA, straight alpha
};

layout(std430, binding = 1) readonly buffer Instances {
    ArcInstance instances[];
};

layout(location = 0) out vec2 v_canvas_pos;
layout(location = 1) out vec2 v_center;
layout(location = 2) out vec2 v_radius_thickness;
layout(location = 3) out vec2 v_angles;
layout(location = 4) out vec4 v_color;

const vec2 CORNERS[6] = vec2[6](
    vec2(0, 0), vec2(1, 0), vec2(0, 1),
    vec2(1, 0), vec2(1, 1), vec2(0, 1)
);

void main() {
    vec2 c = CORNERS[gl_VertexIndex];
    ArcInstance inst = instances[gl_InstanceIndex];
    vec2  cen = inst.center_radius_thickness.xy;
    float r   = inst.center_radius_thickness.z;
    float th  = inst.center_radius_thickness.w;
    float pad = r + th * 0.5 + 1.5;            // +1.5 px AA margin

    vec2 px = cen - vec2(pad) + c * (2.0 * pad);

    float cx = (px.x / viewport.x) * 2.0 - 1.0;
    // Vulkan NDC has +Y pointing down (matches canvas top-left origin).
    float cy = (px.y / viewport.y) * 2.0 - 1.0;

    gl_Position        = vec4(cx, cy, 0.0, 1.0);
    v_canvas_pos       = px;
    v_center           = cen;
    v_radius_thickness = vec2(r, th);
    v_angles           = inst.angles.xy;
    v_color            = inst.color;
}

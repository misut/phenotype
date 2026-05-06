#version 450

// Triangle pipeline vertex shader — direct port of phenotype.native.macos
// vs_tri. Reads the same Uniforms { vec2 viewport; vec2 _pad; } UBO as the
// color pipeline (binding 0) so we share g_renderer.uniform_buffer between
// the two pipelines. Vertex data is real vertex attributes (not an SSBO
// with gl_VertexIndex), bound at draw time via vkCmdBindVertexBuffers.
//
// Pixel→clip math is identical to color.vert (Vulkan NDC +Y-down). Do NOT
// copy the macOS Y-flip; that's a Metal NDC artifact.

layout(std140, binding = 0) uniform Uniforms {
    vec2 viewport;     // swapchain extent in pixels
    vec2 _pad;         // std140 alignment; unused
};

layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec4 a_color;

layout(location = 0) out vec4 v_color;

void main() {
    float cx = (a_pos.x / viewport.x) * 2.0 - 1.0;
    float cy = (a_pos.y / viewport.y) * 2.0 - 1.0;
    gl_Position = vec4(cx, cy, 0.0, 1.0);
    v_color = a_color;
}

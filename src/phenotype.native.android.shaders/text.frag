#version 450

// Text pipeline fragment shader — direct port of phenotype.native.macos
// fs_text. The atlas stores single-channel coverage (R8 alpha from
// android.graphics.Canvas.drawText output). Sample, discard near-zero
// samples to avoid writing dirty pixels from sub-quad runs, modulate
// the quad's color alpha, and emit a premultiplied output that blends
// via the same SRC_ALPHA / ONE_MINUS_SRC_ALPHA pipeline the color draw
// uses.

layout(binding = 2) uniform sampler2D atlas;

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

void main() {
    float coverage = texture(atlas, v_uv).r;
    if (coverage < 0.01) discard;
    frag_color = vec4(v_color.rgb, v_color.a * coverage);
}

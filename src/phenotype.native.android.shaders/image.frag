#version 450

// Image pipeline fragment shader — direct port of phenotype.native.macos
// fs_image. The atlas stores RGBA8 straight-alpha pixels (decoded via
// NDK's AImageDecoder with setUnpremultipliedRequired=true), so the
// pipeline's SRC_ALPHA / ONE_MINUS_SRC_ALPHA blend composites the
// sample against the framebuffer correctly.
//
// The per-instance color acts as a tint: pass vec4(1) for an
// unmodified image; any other value multiplies over the atlas sample.

layout(binding = 2) uniform sampler2D atlas;

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_uv;

layout(location = 0) out vec4 frag_color;

void main() {
    vec4 sample_rgba = texture(atlas, v_uv);
    if (sample_rgba.a < 0.01) discard;
    frag_color = sample_rgba * v_color;
}

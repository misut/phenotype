#version 450

// Triangle pipeline fragment shader — direct port of phenotype.native.macos
// fs_tri. The vertex shader interpolates the per-vertex color across the
// triangle; we just pass it through. No SDF, no border, no atlas.

layout(location = 0) in vec4 v_color;

layout(location = 0) out vec4 frag_color;

void main() {
    frag_color = v_color;
}

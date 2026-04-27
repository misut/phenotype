#version 450

// Arc pipeline — fragment SDF. `radial = |dist(center) - radius|`,
// stroked through smoothstep AA, masked by the [start, end) sweep.
// Mirrors phenotype.native.macos.cppm::fs_arc (MSL).

layout(location = 0) in vec2 v_canvas_pos;
layout(location = 1) in vec2 v_center;
layout(location = 2) in vec2 v_radius_thickness;
layout(location = 3) in vec2 v_angles;
layout(location = 4) in vec4 v_color;

layout(location = 0) out vec4 frag_color;

void main() {
    const float kTwoPi = 6.28318530717958647692;
    vec2  d      = v_canvas_pos - v_center;
    float dist   = length(d);
    float r      = v_radius_thickness.x;
    float th     = v_radius_thickness.y;
    float radial = abs(dist - r);
    float aa     = max(fwidth(radial), 1e-3) * 0.5;
    float alpha_r = 1.0 - smoothstep(th * 0.5 - aa, th * 0.5 + aa, radial);
    if (alpha_r <= 0.0) discard;

    float start = v_angles.x;
    float end   = v_angles.y;
    float sweep = end - start;
    if (sweep <= 0.0)  sweep += kTwoPi;
    if (sweep > kTwoPi) sweep = kTwoPi;

    // Full-circle fast path. CIRCLE entities (sweep ≈ 2π) are the
    // dominant case for cad++ DWG rendering, and their angle test is
    // tautological. Skipping it cuts the per-fragment work in half on
    // mobile GPUs (no atan, no floor, no extra branch).
    if (sweep < kTwoPi - 1e-3) {
        float angle = atan(d.y, d.x);   // [-π, π]
        float relative = angle - start;
        relative = relative - kTwoPi * floor(relative / kTwoPi);
        if (relative > sweep) discard;
    }

    frag_color = vec4(v_color.rgb, v_color.a * alpha_r);
}

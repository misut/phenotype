#version 450

// Color pipeline fragment shader — direct port of phenotype.native.macos
// fs_color / phenotype.native.windows fs_color. Single-pipeline design:
// params.z selects the draw path (0 = Fill, 1 = Stroke, 2 = Round, 3
// falls through to Fill to match desktop DrawLine behavior).

layout(location = 0) in vec4 v_color;
layout(location = 1) in vec2 v_local_pos;
layout(location = 2) in vec2 v_rect_size;
layout(location = 3) in vec4 v_params;

layout(location = 0) out vec4 frag_color;

void main() {
    uint  draw_type = uint(v_params.z);
    float radius    = v_params.x;
    float border_w  = v_params.y;

    // Rounded rect (SDF) — quarter-circle distance field with 1-pixel AA.
    if (draw_type == 2u && radius > 0.0) {
        vec2 half_size = v_rect_size * 0.5;
        vec2 p = abs(v_local_pos - half_size) - half_size + vec2(radius);
        float d = length(max(p, vec2(0.0))) - radius;
        if (d > 0.5) discard;
        float alpha = v_color.a * clamp(0.5 - d, 0.0, 1.0);
        frag_color = vec4(v_color.rgb * alpha, alpha);
        return;
    }

    // Stroke rect — discard pixels in the interior band.
    if (draw_type == 1u) {
        vec2 lp = v_local_pos;
        vec2 sz = v_rect_size;
        if (lp.x > border_w && lp.x < sz.x - border_w &&
            lp.y > border_w && lp.y < sz.y - border_w) discard;
        frag_color = v_color;
        return;
    }

    // Fill (and DrawLine fall-through): plain color.
    frag_color = v_color;
}

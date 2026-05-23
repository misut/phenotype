#version 450

// Color pipeline fragment shader — direct port of phenotype.native.macos
// fs_color / phenotype.native.windows fs_color. Single-pipeline design:
// params.z selects the draw path (0 = Fill, 1 = Stroke, 2 = Round,
// 3 = rounded-edge material stroke, 5 = soft-edge rounded fill).

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

    // Feathered rounded rect. The pure MaterialPlan resolves the
    // padding and soft-edge radius; this shader only applies the alpha
    // falloff inside the inflated paint bounds.
    if (draw_type == 5u && radius > 0.0) {
        float soft_edge = max(v_params.w, 0.5);
        vec2 half_size = v_rect_size * 0.5;
        vec2 p = abs(v_local_pos - half_size) - half_size + vec2(radius);
        float d = length(max(p, vec2(0.0))) - radius;
        if (d > 0.5) discard;
        float outer = clamp(0.5 - d, 0.0, 1.0);
        float feather = clamp((-d) / soft_edge, 0.0, 1.0);
        float alpha = v_color.a * min(outer, feather);
        frag_color = vec4(v_color.rgb * alpha, alpha);
        return;
    }

    // Rounded material edge stroke. The pure MaterialPlan owns the
    // stroke width; this shader only executes the resolved layer.
    if (draw_type == 3u && radius > 0.0) {
        vec2 half_size = v_rect_size * 0.5;
        vec2 p = abs(v_local_pos - half_size) - half_size + vec2(radius);
        float d = length(max(p, vec2(0.0))) - radius;
        if (d > 0.5 || d < -border_w - 0.5) discard;
        float outer = clamp(0.5 - d, 0.0, 1.0);
        float inner = clamp(d + border_w + 0.5, 0.0, 1.0);
        float alpha = v_color.a * min(outer, inner);
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

    if (draw_type == 3u) {
        vec2 lp = v_local_pos;
        vec2 sz = v_rect_size;
        if (lp.x > border_w && lp.x < sz.x - border_w &&
            lp.y > border_w && lp.y < sz.y - border_w) discard;
        float alpha = v_color.a;
        frag_color = vec4(v_color.rgb * alpha, alpha);
        return;
    }

    // Fill: plain color.
    frag_color = v_color;
}

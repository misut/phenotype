// phenotype WGSL shaders — extracted verbatim from shim/phenotype.js.
// Used by both the JS WebGPU renderer and the native Dawn renderer.

struct Uniforms {
  viewport: vec2f,
  _pad: vec2f,
};

@group(0) @binding(0) var<uniform> uniforms: Uniforms;

// --- Color pipeline ---

struct ColorInstance {
  @location(0) rect: vec4f,    // x, y, w, h
  @location(1) color: vec4f,   // r, g, b, a
  @location(2) params: vec4f,  // corner_radius, border_width, type(0=fill,1=stroke,2=round,3=line), 0
};

struct ColorVsOut {
  @builtin(position) pos: vec4f,
  @location(0) color: vec4f,
  @location(1) local_pos: vec2f,
  @location(2) rect_size: vec2f,
  @location(3) params: vec4f,
};

@vertex fn vs_color(
  @builtin(vertex_index) vi: u32,
  @builtin(instance_index) ii: u32,
  inst: ColorInstance,
) -> ColorVsOut {
  var corners = array<vec2f, 6>(
    vec2f(0, 0), vec2f(1, 0), vec2f(0, 1),
    vec2f(1, 0), vec2f(1, 1), vec2f(0, 1),
  );
  let c = corners[vi];
  let px = inst.rect.x + c.x * inst.rect.z;
  let py = inst.rect.y + c.y * inst.rect.w;
  let cx = (px / uniforms.viewport.x) * 2.0 - 1.0;
  let cy = 1.0 - (py / uniforms.viewport.y) * 2.0;

  var out: ColorVsOut;
  out.pos = vec4f(cx, cy, 0, 1);
  out.color = inst.color;
  out.local_pos = c * inst.rect.zw;
  out.rect_size = inst.rect.zw;
  out.params = inst.params;
  return out;
}

@fragment fn fs_color(in: ColorVsOut) -> @location(0) vec4f {
  let draw_type = u32(in.params.z);
  let radius = in.params.x;
  let border_w = in.params.y;

  if (draw_type == 2u && radius > 0.0) {
    let half_size = in.rect_size * 0.5;
    let p = abs(in.local_pos - half_size) - half_size + vec2f(radius);
    let d = length(max(p, vec2f(0.0))) - radius;
    if (d > 0.5) { discard; }
    let alpha = in.color.a * clamp(0.5 - d, 0.0, 1.0);
    return vec4f(in.color.rgb * alpha, alpha);
  }

  if (draw_type == 1u) {
    let lp = in.local_pos;
    let sz = in.rect_size;
    let bw = border_w;
    if (lp.x > bw && lp.x < sz.x - bw && lp.y > bw && lp.y < sz.y - bw) {
      discard;
    }
    return in.color;
  }

  if (draw_type == 3u) {
    return in.color;
  }

  return in.color;
}

// --- Texture pipeline (text + images) ---

struct TextInstance {
  @location(0) rect: vec4f,
  @location(1) uv_rect: vec4f,
};

struct TextVsOut {
  @builtin(position) pos: vec4f,
  @location(0) uv: vec2f,
};

@vertex fn vs_text(
  @builtin(vertex_index) vi: u32,
  inst: TextInstance,
) -> TextVsOut {
  var corners = array<vec2f, 6>(
    vec2f(0, 0), vec2f(1, 0), vec2f(0, 1),
    vec2f(1, 0), vec2f(1, 1), vec2f(0, 1),
  );
  let c = corners[vi];
  let px = inst.rect.x + c.x * inst.rect.z;
  let py = inst.rect.y + c.y * inst.rect.w;
  let cx = (px / uniforms.viewport.x) * 2.0 - 1.0;
  let cy = 1.0 - (py / uniforms.viewport.y) * 2.0;

  var out: TextVsOut;
  out.pos = vec4f(cx, cy, 0, 1);
  out.uv = inst.uv_rect.xy + c * inst.uv_rect.zw;
  return out;
}

@group(0) @binding(1) var textAtlas: texture_2d<f32>;
@group(0) @binding(2) var textSampler: sampler;

@fragment fn fs_text(in: TextVsOut) -> @location(0) vec4f {
  let sample = textureSample(textAtlas, textSampler, in.uv);
  if (sample.a < 0.01) { discard; }
  return sample;
}

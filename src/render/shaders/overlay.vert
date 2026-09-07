#version 450
layout(location = 0) out vec2 v_uv;
layout(location = 1) out vec4 v_colour;
layout(location = 2) out vec4 v_clip;
layout(location = 3) out vec2 v_px;
layout(location = 4) out vec2 v_centre;
layout(location = 5) out vec2 v_halfSize;
layout(location = 6) out float v_radius;
layout(location = 7) out float v_hasPatch;
layout(location = 0) in vec4 a_rect;
layout(location = 1) in vec4 a_uv;
layout(location = 2) in vec4 a_colour;
layout(location = 3) in vec4 a_clip;
layout(location = 4) in vec4 a_shape;
layout(std140, set = 1, binding = 0) uniform Frame { vec2 targetPx; float encodesSrgb; float pad; } f;
vec3 asLinear(vec3 code) {
  return mix(code / 12.92, pow((code + 0.055) / 1.055, vec3(2.4)), greaterThan(code, vec3(0.04045)));
}

void main() {

  vec2 corner[6] = vec2[6]( vec2(0,0), vec2(1,0), vec2(0,1),
                       vec2(0,1), vec2(1,0), vec2(1,1) );
  vec2 at = corner[gl_VertexIndex];
  vec2 px = a_rect.xy + at * a_rect.zw;
  gl_Position = vec4(px.x / f.targetPx.x * 2.0 - 1.0, 1.0 - px.y / f.targetPx.y * 2.0, 0.0, 1.0);
  v_uv = mix(a_uv.xy, a_uv.zw, at);
  v_colour = a_colour;
  v_clip = a_clip;
  v_px = px;
  v_centre = a_rect.xy + a_rect.zw * 0.5;
  v_halfSize = a_rect.zw * 0.5;
  v_radius = min(a_shape.x, min(v_halfSize.x, v_halfSize.y));
  v_colour.a *= a_shape.y;
  v_colour.rgb = f.encodesSrgb > 0.5 ? asLinear(v_colour.rgb) : v_colour.rgb;
  v_hasPatch = (a_uv.z > a_uv.x && a_uv.w > a_uv.y) ? 1.0 : 0.0;
}

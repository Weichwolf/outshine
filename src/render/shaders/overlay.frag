#version 450
layout(location = 0) in vec2 v_uv;
layout(location = 1) in vec4 v_colour;
layout(location = 2) in vec4 v_clip;
layout(location = 3) in vec2 v_px;
layout(location = 4) in vec2 v_centre;
layout(location = 5) in vec2 v_halfSize;
layout(location = 6) in float v_radius;
layout(location = 7) in float v_hasPatch;
layout(set = 2, binding = 0) uniform sampler2D atlas;
layout(location = 0) out vec4 colour;

void main() {

  if (v_px.x < v_clip.x || v_px.x > v_clip.x + v_clip.z ||
      v_px.y < v_clip.y || v_px.y > v_clip.y + v_clip.w) {
    discard;
  }
  vec4 shaded = v_colour;
  if (v_hasPatch > 0.5) { shaded *= texture(atlas, v_uv); }
  if (v_radius > 0.0) {
    vec2 q = abs(v_px - v_centre) - (v_halfSize - v_radius);
    float d = length(max(q, vec2(0.0))) + min(max(q.x, q.y), 0.0) - v_radius;
    shaded.a *= clamp(0.5 - d, 0.0, 1.0);
  }
  if (shaded.a <= 0.0) { discard; }
  colour = vec4(shaded.rgb * shaded.a, shaded.a);
}

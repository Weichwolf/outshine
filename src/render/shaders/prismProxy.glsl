vec4 prismProxyPosition(PrismPatch gridPatch, mat4 clipFromLocal, uint vertex) {
  vec2 lower = vec2(1.0), upper = vec2(-1.0);
  bool crossesEye = false;
  uint outside = 63u;
  for (uint corner = 0u; corner < 8u; ++corner) {
    vec3 fraction = vec3(float(corner & 1u), float((corner >> 1u) & 1u), float(corner >> 2u));
    vec3 point = fraction * vec3(gridPatch.bounds.y, gridPatch.bounds.x - gridPatch.shape.w, gridPatch.bounds.z);
    point.y += gridPatch.shape.w;
    vec4 clip = clipFromLocal * vec4(point, 1.0);
    if (any(isnan(clip)) || any(isinf(clip))) {
      crossesEye = true;
      outside = 0u;
      continue;
    }
    uint code = (clip.x < -clip.w ? 1u : 0u) | (clip.x > clip.w ? 2u : 0u)
                  | (clip.y < -clip.w ? 4u : 0u) | (clip.y > clip.w ? 8u : 0u)
                  | (clip.z < 0.0 ? 16u : 0u) | (clip.z > clip.w ? 32u : 0u);
    outside &= code;
    if (clip.w <= 0.0) {
      crossesEye = true;
    } else {
      vec2 projected = clamp(clip.xy / clip.w, vec2(-1.0), vec2(1.0));
      lower = min(lower, projected);
      upper = max(upper, projected);
    }
  }
  if (outside != 0u) { return vec4(2.0, 2.0, 0.5, 1.0); }
  if (crossesEye) {
    lower = vec2(-1.0);
    upper = vec2(1.0);
  }
  const vec2 corners[6] = vec2[6](vec2(0, 0), vec2(0, 1), vec2(1, 0),
                                  vec2(1, 0), vec2(0, 1), vec2(1, 1));
  // Bounded proxy clipping preserves coverage; only the analytic trace writes actual depth.
  return vec4(mix(lower, upper, corners[vertex]), 0.5, 1.0);
}

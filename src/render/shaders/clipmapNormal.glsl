#include "clipmapRings.glsl"
layout(set = 2, binding = 4) uniform sampler2DArray clipmapNormals;

vec3 clipmapNormalAt(vec2 grid, ClipmapRing ring) {
  vec2 texel = 2.0 * grid + vec2(ring.Offsets.zw + ring.NormalAddress.xy) + vec2(0.5);
  return textureLod(clipmapNormals,
      vec3(texel / float(ring.NormalAddress.z), float(ring.NormalAddress.w)), 0.0).xyz;
}

vec3 clipmapNormal() {
  ClipmapRing ring = clipmapRings[clipmapLevel];
  vec3 fine = clipmapNormalAt(clipmapGrid, ring);
  float weight = clipmapMorph(clipmapGrid, ring.Morph);
  if (ring.CentreStep.w > 0.0 && weight > 0.0) {
    vec2 parent = 0.5 * clipmapGrid + vec2(ring.Offsets.xy);
    vec3 coarse = clipmapNormalAt(parent, clipmapRings[clipmapLevel + 1u]);
    fine = mix(fine, coarse, weight);
  }
  return normalize(vec3(fine.x, fine.z, -fine.y));
}

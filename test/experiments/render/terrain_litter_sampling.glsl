#include "fieldFootprint.glsl"

struct LitterSample {
  vec4 ValueLodCountFade;
  vec4 LengthsSpanFootprint;
};

float litterTexel(sampler2D field, vec2 uv, float lod, bool explicitMipBlend) {
  if (!explicitMipBlend) { return textureLod(field, uv, lod).r; }
  float base = floor(lod);
  float first = textureLod(field, uv, base).r;
  if (fract(lod) == 0.0) { return first; }
  float next = min(base + 1.0, float(textureQueryLevels(field) - 1));
  return mix(first, textureLod(field, uv, next).r, fract(lod));
}

LitterSample sampleLitterTexture(sampler2D field,
                                 vec2 uv,
                                 vec2 dx,
                                 vec2 dy,
                                 float scale,
                                 float lodBias,
                                 uint maximumTaps,
                                 bool explicitMipBlend) {
  FieldFootprint plan = fieldFootprint(dx, dy, scale, float(textureSize(field, 0).x),
                                      lodBias, float(textureQueryLevels(field) - 1), maximumTaps);
  if (plan.Count == 0u) { return LitterSample(vec4(0.0), vec4(0.0)); }
  float total = 0.0;
  for (uint at = 0u; at < plan.Count; ++at) {
    float offset = (float(at) + 0.5) / float(plan.Count) - 0.5;
    total += litterTexel(field, uv + offset * plan.Span * plan.Axis, plan.Lod, explicitMipBlend);
  }
  return LitterSample(vec4((total / float(plan.Count) - 0.5) * plan.Fade, plan.Lod, float(plan.Count), plan.Fade),
                      vec4(plan.Major, plan.Minor, plan.Span, plan.Footprint));
}

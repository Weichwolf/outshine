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
  float footprint = max(length(dx), length(dy));
  if (footprint >= 2.0 * scale) { return LitterSample(vec4(0.0), vec4(0.0)); }
  float a = dx.x * dx.x + dy.x * dy.x;
  float b = dx.x * dx.y + dy.x * dy.y;
  float c = dx.y * dx.y + dy.y * dy.y;
  float separation = length(vec2(a - c, 2.0 * b));
  float major2 = max(0.5 * (a + c + separation), 0.0);
  float minor2 = max(0.5 * (a + c - separation), 0.0);
  float major = sqrt(major2), minor = sqrt(minor2);
  vec2 axis = a >= c ? vec2(major2 - c, b) : vec2(b, major2 - a);
  axis = dot(axis, axis) > 0.0 ? normalize(axis) : vec2(1.0, 0.0);
  float aspect = minor > 0.0 ? major / minor : float(maximumTaps);
  uint count = uint(ceil(clamp(aspect, 1.0, float(maximumTaps))));
  if (major <= scale * 0.01) { count = 1u; }
  float effectiveMinor = max(minor, major / float(count));
  float span = sqrt(max(major2 - effectiveMinor * effectiveMinor, 0.0));
  float texels = float(textureSize(field, 0).x);
  float lod = clamp(
      log2(max(effectiveMinor * texels, 1.0)) + lodBias, 0.0, float(textureQueryLevels(field) - 1));
  float total = 0.0;
  for (uint at = 0u; at < count; ++at) {
    float offset = (float(at) + 0.5) / float(count) - 0.5;
    total += litterTexel(field, uv + offset * span * axis, lod, explicitMipBlend);
  }
  float fade = 1.0 - smoothstep(1.0, 2.0, footprint / scale);
  return LitterSample(vec4((total / float(count) - 0.5) * fade, lod, float(count), fade),
                      vec4(major, minor, span, footprint));
}

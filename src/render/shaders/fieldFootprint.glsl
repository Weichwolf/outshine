#ifndef OUTSHINE_FIELD_FOOTPRINT_GLSL
#define OUTSHINE_FIELD_FOOTPRINT_GLSL
struct FieldFootprint {
  vec2 Axis;
  float Major;
  float Minor;
  float Span;
  float Footprint;
  float Lod;
  uint Count;
  float Fade;
};

FieldFootprint fieldFootprint(vec2 dx, vec2 dy, float scale, float texels,
                              float lodBias, float lastMip, uint maximumTaps) {
  float footprint = max(length(dx), length(dy));
  if (footprint >= 2.0 * scale) {
    return FieldFootprint(vec2(1.0, 0.0), 0.0, 0.0, 0.0, footprint, 0.0, 0u, 0.0);
  }
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
  float lod = clamp(log2(max(effectiveMinor * texels, 1.0)) + lodBias, 0.0, lastMip);
  float fade = 1.0 - smoothstep(1.0, 2.0, footprint / scale);
  return FieldFootprint(axis, major, minor, span, footprint, lod, count, fade);
}
#endif

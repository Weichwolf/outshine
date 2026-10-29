#include "radiancePair.glsl"

vec4 radianceSample(sampler2D image, vec2 uv, float scattering) {
  if (scattering != 3.0) { return texture(image, uv); }
  vec2 raster = vec2(textureSize(image, 0).x / 2 - 1, textureSize(image, 0).y - 1);
  vec2 at = uv * raster;
  vec2 halfSpan = .5 * (abs(dFdx(at)) + abs(dFdy(at)));
  vec2 low = at - halfSpan, high = at + halfSpan;
  float area = 4.0 * halfSpan.x * halfSpan.y;
  if (!(area > 0.0)) { return vec4(0.0); }
  RadiancePair a = radiancePrefixPair(image, high), b = radiancePrefixPair(image, low);
  RadiancePair c = radiancePrefixPair(image, vec2(low.x, high.y));
  RadiancePair d = radiancePrefixPair(image, vec2(high.x, low.y));
  RadiancePair result =
      radianceAddPair(radianceAddPair(radianceAddPair(a, b), RadiancePair(-c.high, -c.low)),
                      RadiancePair(-d.high, -d.low));
  precise vec4 total = result.high + result.low;
  vec4 value = total / area;
  value.a = clamp(value.a, 0.0, 1.0);
  value.rgb = value.a > 0.0 ? max(value.rgb, vec3(0.0)) : vec3(0.0);
  return value;
}

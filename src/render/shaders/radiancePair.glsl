#ifndef OUTSHINE_RADIANCE_PAIR_GLSL
#define OUTSHINE_RADIANCE_PAIR_GLSL

struct RadiancePair {
  vec4 high;
  vec4 low;
};

vec4 radianceRoundedAdd(vec4 a, vec4 b) {
#ifdef RADIANCE_PAIR_STRICT_COMPUTE
  precise vec4 result = fma(vec4(1.0), a, b);
#else
  precise vec4 result = a + b;
#endif
  return result;
}

vec4 radianceRoundedSubtract(vec4 a, vec4 b) {
#ifdef RADIANCE_PAIR_STRICT_COMPUTE
  precise vec4 result = fma(vec4(-1.0), b, a);
#else
  precise vec4 result = a - b;
#endif
  return result;
}

RadiancePair radianceTwoSum(vec4 a, vec4 b) {
  precise vec4 total = radianceRoundedAdd(a, b);
  precise vec4 back = radianceRoundedSubtract(total, a);
  precise vec4 error = radianceRoundedAdd(
      radianceRoundedSubtract(a, radianceRoundedSubtract(total, back)),
      radianceRoundedSubtract(b, back));
  return RadiancePair(total, error);
}

RadiancePair radianceAddPair(RadiancePair a, RadiancePair b) {
  RadiancePair high = radianceTwoSum(a.high, b.high);
  RadiancePair low = radianceTwoSum(a.low, b.low);
  RadiancePair middle = radianceTwoSum(high.low, low.high);
  RadiancePair combined = radianceTwoSum(high.high, middle.high);
  precise vec4 tail = radianceRoundedAdd(radianceRoundedAdd(low.low, middle.low), combined.low);
  return radianceTwoSum(combined.high, tail);
}

RadiancePair radianceScalePair(RadiancePair value, float weight) {
  precise vec4 high = value.high * weight;
  precise vec4 residual = fma(value.high, vec4(weight), -high);
  precise vec4 low = residual + value.low * weight;
  return radianceTwoSum(high, low);
}

RadiancePair radianceMixPair(RadiancePair a, RadiancePair b, float weight) {
  precise float other = 1.0 - weight;
  return radianceAddPair(radianceScalePair(a, other), radianceScalePair(b, weight));
}

RadiancePair radianceLoadPair(sampler2D image, ivec2 at) {
  return RadiancePair(texelFetch(image, ivec2(at.x * 2, at.y), 0),
                      texelFetch(image, ivec2(at.x * 2 + 1, at.y), 0));
}

RadiancePair radiancePrefixPair(sampler2D image, vec2 at) {
  ivec2 most = ivec2(textureSize(image, 0).x / 2 - 1, textureSize(image, 0).y - 1);
  vec2 point = clamp(at, vec2(0.0), vec2(most));
  ivec2 low = ivec2(floor(point));
  ivec2 high = min(low + 1, most);
  vec2 fraction = fract(point);
  return radianceMixPair(
      radianceMixPair(
          radianceLoadPair(image, low), radianceLoadPair(image, ivec2(high.x, low.y)), fraction.x),
      radianceMixPair(
          radianceLoadPair(image, ivec2(low.x, high.y)), radianceLoadPair(image, high), fraction.x),
      fraction.y);
}
#endif

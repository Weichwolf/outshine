vec4 temporalCubicWeights(float phase) {
  float square = phase * phase;
  float cube = square * phase;
  return vec4(square - 0.5 * (phase + cube),
              1.0 - 2.5 * square + 1.5 * cube,
              0.5 * phase + 2.0 * square - 1.5 * cube,
              0.5 * (cube - square));
}

vec4 temporalHistorySample(sampler2D image, vec2 uv, vec2 texel) {
  vec2 position = uv / texel;
  vec2 base = floor(position - 0.5) + 0.5;
  vec4 xWeights = temporalCubicWeights(position.x - base.x);
  vec4 yWeights = temporalCubicWeights(position.y - base.y);
  vec2 middleWeight = vec2(xWeights.y + xWeights.z, yWeights.y + yWeights.z);
  vec2 middle = (base + vec2(xWeights.z, yWeights.z) / middleWeight) * texel;
  vec2 low = (base - 1.0) * texel;
  vec2 high = (base + 2.0) * texel;
  vec4 samples[5] = vec4[](textureLod(image, vec2(middle.x, low.y), 0.0),
                          textureLod(image, vec2(low.x, middle.y), 0.0),
                          textureLod(image, middle, 0.0),
                          textureLod(image, vec2(high.x, middle.y), 0.0),
                          textureLod(image, vec2(middle.x, high.y), 0.0));
  float weights[5] = float[](middleWeight.x * yWeights.x,
                            xWeights.x * middleWeight.y,
                            middleWeight.x * middleWeight.y,
                            xWeights.w * middleWeight.y,
                            middleWeight.x * yWeights.w);
  vec4 result = vec4(0.0);
  vec4 lowest = samples[0];
  vec4 highest = samples[0];
  float weight = 0.0;
  for (int i = 0; i < 5; ++i) {
    result += samples[i] * weights[i];
    weight += weights[i];
    lowest = min(lowest, samples[i]);
    highest = max(highest, samples[i]);
  }
  return clamp(result / weight, lowest, highest);
}

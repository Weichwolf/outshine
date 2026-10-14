vec2 blendBumpDerivatives(vec2 a, vec2 b, float factor) {
  return a * (1.0f - factor) + b * factor;
}

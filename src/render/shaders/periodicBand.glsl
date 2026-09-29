float bandIntegral(float coordinate, float low, float high) {
  float phase = fract(coordinate);
  return clamp(phase - low, 0.0f, high - low) - phase * (high - low);
}

float periodicBand(float coordinate, float low, float high, float width) {
  float span = max(width, 0.0001f);
  return clamp(high - low +
      (bandIntegral(coordinate + 0.5f * span, low, high) -
       bandIntegral(coordinate - 0.5f * span, low, high)) / span, 0.0f, 1.0f);
}

float intervalBand(float coordinate, float low, float high, float width) {
  float span = max(width, 0.0001f);
  return clamp((min(coordinate + 0.5f * span, high) -
                max(coordinate - 0.5f * span, low)) / span, 0.0f, 1.0f);
}

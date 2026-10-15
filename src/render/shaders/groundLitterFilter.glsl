struct GroundLitterAxis {
  vec4 Weights;
  int First;
};

float groundLitterPrimitive(float at) {
  return at * at * at * (1.0 - 0.5 * at);
}

GroundLitterAxis groundLitterAxis(float centre, float width) {
  if (width <= 0.01) {
    float fraction = fract(centre);
    float blend = fraction * fraction * (3.0 - 2.0 * fraction);
    return GroundLitterAxis(vec4(1.0 - blend, blend, 0.0, 0.0), int(floor(centre)));
  }
  float halfWidth = 0.5 * width;
  int first = int(floor(centre - halfWidth));
  vec4 weights = vec4(0.0);
  for (int cell = 0; cell < 3; ++cell) {
    float low = clamp(centre - halfWidth - float(first + cell), 0.0f, 1.0f);
    float high = clamp(centre + halfWidth - float(first + cell), 0.0f, 1.0f);
    float upper = (groundLitterPrimitive(high) - groundLitterPrimitive(low)) / width;
    weights[cell] += (high - low) / width - upper;
    weights[cell + 1] += upper;
  }
  return GroundLitterAxis(weights, first);
}

float groundLitterField(vec2 atM, vec2 widthM, float scaleM) {
  vec2 width = widthM / scaleM;
  float footprint = max(width.x, width.y);
  if (footprint >= 2.0) { return 0.0; }
  vec2 at = atM / scaleM;
  ivec2 origin = ivec2(floor(at));
  vec2 fraction = fract(at);
  GroundLitterAxis horizontal = groundLitterAxis(fraction.x, width.x);
  GroundLitterAxis vertical = groundLitterAxis(fraction.y, width.y);
  ivec2 first = origin + ivec2(horizontal.First, vertical.First);
  float total = 0.0;
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      float weight = horizontal.Weights[x] * vertical.Weights[y];
      if (weight > 0.0) { total += weight * surfaceValue(ivec3(first + ivec2(x, y), 0)); }
    }
  }
  return (total - 0.5) * (1.0 - smoothstep(1.0, 2.0, footprint));
}

#ifndef GROUND_LITTER_GAUSS_POINTS
#define GROUND_LITTER_GAUSS_POINTS 0
#endif

#if GROUND_LITTER_GAUSS_POINTS != 0 && GROUND_LITTER_GAUSS_POINTS != 2 &&                          \
    GROUND_LITTER_GAUSS_POINTS != 3
#error GROUND_LITTER_GAUSS_POINTS must be 0, 2 or 3
#endif

float groundLitterFootprintField(vec2 atM, vec2 worldDx, vec2 worldDy, float scaleM) {
#if GROUND_LITTER_GAUSS_POINTS == 0
  return groundLitterField(atM, abs(worldDx) + abs(worldDy), scaleM);
#else
  float footprint = max(length(worldDx), length(worldDy)) / scaleM;
  if (footprint >= 2.0) { return 0.0; }
  vec2 at = atM / scaleM;
  if (footprint <= 0.01) { return surfaceNoise2(at).z - 0.5; }
  vec2 dx = worldDx / scaleM;
  vec2 dy = worldDy / scaleM;
  float total = 0.0;
#if GROUND_LITTER_GAUSS_POINTS == 2
  const float offset = 0.28867513459481287;
  total = 0.25 * (surfaceNoise2(at - offset * dx - offset * dy).z +
                  surfaceNoise2(at + offset * dx - offset * dy).z +
                  surfaceNoise2(at - offset * dx + offset * dy).z +
                  surfaceNoise2(at + offset * dx + offset * dy).z);
#else
  const vec3 offsets = vec3(-0.3872983346207417, 0.0, 0.3872983346207417);
  const vec3 weights = vec3(5.0, 8.0, 5.0) / 18.0;
  for (int y = 0; y < 3; ++y) {
    for (int x = 0; x < 3; ++x) {
      total += weights[x] * weights[y] * surfaceNoise2(at + offsets[x] * dx + offsets[y] * dy).z;
    }
  }
#endif
  return (total - 0.5) * (1.0 - smoothstep(1.0, 2.0, footprint));
#endif
}

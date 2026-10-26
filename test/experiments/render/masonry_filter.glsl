float masonryBand(float at, float low, float high, float width) {
  vec2 ends = fract(at) + vec2(-0.5, 0.5) * max(width, 0.0001);
  if (ends.x >= low && ends.y <= high) { return 1.0; }
  if ((ends.x >= 0.0 && ends.y <= low) || (ends.x >= high && ends.y <= 1.0)) { return 0.0; }
  return periodicBand(at, low, high, width);
}

#ifndef MASONRY_STABLE_PHASE
#define MASONRY_STABLE_PHASE 1
#endif

vec2 masonryPhase(vec2 atM) {
  const vec2 repeatsPerM = vec2(4.0, 6.25);
#if MASONRY_STABLE_PHASE
  return fract(fma(atM, repeatsPerM, -floor(atM * repeatsPerM)));
#else
  return atM * repeatsPerM;
#endif
}

float masonryBrickBox(vec2 at, vec2 width) {
  float columns = masonryBand(at.x, 0.02, 0.98, width.x);
  float shifted = masonryBand(at.x - 0.5, 0.02, 0.98, width.x);
  float even = masonryBand(at.y, 0.03125, 0.46875, width.y);
  float odd = masonryBand(at.y, 0.53125, 0.96875, width.y);
  return clamp(columns * even + shifted * odd, 0.0, 1.0);
}

float masonryBrickPoint(vec2 at) {
  at = fract(at);
  float column = fract(at.x - (at.y >= 0.5 ? 0.5 : 0.0));
  float row = mod(at.y, 0.5);
  return column > 0.02 && column < 0.98 && row > 0.03125 && row < 0.46875 ? 1.0 : 0.0;
}

float masonryWallWeight(vec3 coverage) {
  return max(1.0 - coverage.x - 0.35 * coverage.z, 0.0);
}

vec3 masonryCoverage(vec2 pointM, vec2 widthM, vec4 profile) {
  float perM = profile.z > 0.0 && profile.w > 0.0 ? 1.0 / profile.y : 0.0;
  return facadeBoxCoverage(profile.zw, pointM / profile.xy, widthM / profile.xy, perM);
}

vec4 masonryFootprint(vec2 atM, vec2 dxM, vec2 dyM, vec4 profile) {
  float perM = profile.z > 0.0 && profile.w > 0.0 ? 1.0 / profile.y : 0.0;
  vec3 regions = facadeFootprintCoverage(profile.zw, atM / profile.xy,
                                         dxM / profile.xy, dyM / profile.xy, perM);
  float wall = masonryWallWeight(regions);
  vec2 widthM = abs(dxM) + abs(dyM);
  vec2 brickAt = masonryPhase(atM);
  vec2 brickDx = dxM * vec2(4.0, 6.25);
  vec2 brickDy = dyM * vec2(4.0, 6.25);
  vec2 brickWidth = abs(brickDx) + abs(brickDy);
#if MASONRY_METHOD == 6
  float field = 0.0;
#elif MASONRY_METHOD == 0
  float field = (0.84 - masonryBrickPoint(brickAt)) * wall;
#elif MASONRY_METHOD == 1
  float field = (0.84 - masonryBrickBox(brickAt, brickWidth)) * wall;
#else
#if MASONRY_METHOD == 2
  const int side = 1;
#elif MASONRY_METHOD == 3
  const int side = 2;
#elif MASONRY_METHOD == 4
  const int side = 3;
#else
  const int side = 4;
#endif
  float field = 0.0;
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      vec2 offset = (vec2(x, y) + 0.5) / float(side) - 0.5;
      vec2 pointM = atM + offset.x * dxM + offset.y * dyM;
      float weight = masonryWallWeight(masonryCoverage(pointM, widthM / float(side), profile));
#if MASONRY_STABLE_PHASE
      vec2 brickPoint = brickAt + offset.x * brickDx + offset.y * brickDy;
      vec2 cellWidth = brickWidth / float(side);
#else
      vec2 brickPoint = pointM / vec2(0.25, 0.16);
      vec2 cellWidth = widthM / (float(side) * vec2(0.25, 0.16));
#endif
      field += (0.84 - masonryBrickBox(brickPoint, cellWidth)) * weight;
    }
  }
  field /= float(side * side);
#endif
  return vec4(regions, clamp(field, -0.16 * wall, 0.84 * wall));
}

vec3 masonryAmplitude(vec3 albedo) {
  vec3 desired = (vec3(0.48, 0.46, 0.42) - albedo) / 0.84;
  vec3 positive = min(albedo / 0.16, (vec3(1.0) - albedo) / 0.84);
  vec3 negative = min(albedo / 0.84, (vec3(1.0) - albedo) / 0.16);
  return clamp(desired, -max(negative, vec3(0.0)), max(positive, vec3(0.0)));
}

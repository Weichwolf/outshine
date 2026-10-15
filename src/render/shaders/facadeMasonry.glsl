float facadeMasonryBand(float at, float low, float high, float width) {
  vec2 ends = fract(at) + vec2(-0.5, 0.5) * max(width, 0.0001);
  if (ends.x >= low && ends.y <= high) { return 1.0; }
  if ((ends.x >= 0.0 && ends.y <= low) || (ends.x >= high && ends.y <= 1.0)) { return 0.0; }
  return periodicBand(at, low, high, width);
}

float facadeMasonryBrickBox(vec2 at, vec2 width) {
  float columns = facadeMasonryBand(at.x, 0.02, 0.98, width.x);
  float shifted = facadeMasonryBand(at.x - 0.5, 0.02, 0.98, width.x);
  float even = facadeMasonryBand(at.y, 0.03125, 0.46875, width.y);
  float odd = facadeMasonryBand(at.y, 0.53125, 0.96875, width.y);
  return clamp(columns * even + shifted * odd, 0.0, 1.0);
}

float facadeMasonryWall(vec3 coverage) {
  return max(1.0 - coverage.x - 0.35 * coverage.z, 0.0);
}

float facadeMasonryField(vec2 fractions, vec2 at, vec2 dx, vec2 dy, float storeysPerM,
                         vec2 atM, vec2 dxM, vec2 dyM, vec3 regions) {
  at = fract(at);
  const vec2 repeatsPerM = vec2(4.0, 6.25);
  vec2 brickAt = fract(fma(atM, repeatsPerM, -floor(atM * repeatsPerM)));
  vec2 brickDx = dxM * repeatsPerM;
  vec2 brickDy = dyM * repeatsPerM;
  vec2 brickWidth = (abs(brickDx) + abs(brickDy)) / 4.0;
  vec2 width = (abs(dx) + abs(dy)) / 4.0;
  float field = 0.0;
  for (int y = 0; y < 4; ++y) {
    for (int x = 0; x < 4; ++x) {
      vec2 offset = (vec2(x, y) + 0.5) / 4.0 - 0.5;
      vec2 point = at + offset.x * dx + offset.y * dy;
      float wall = facadeMasonryWall(facadeBoxCoverage(fractions, point, width, storeysPerM));
      vec2 brick = brickAt + offset.x * brickDx + offset.y * brickDy;
      field += (0.84 - facadeMasonryBrickBox(brick, brickWidth)) * wall;
    }
  }
  float wall = facadeMasonryWall(regions);
  return clamp(field / 16.0, -0.16 * wall, 0.84 * wall);
}

vec3 facadeMasonryAmplitude(vec3 albedo) {
  vec3 desired = (vec3(0.48, 0.46, 0.42) - albedo) / 0.84;
  vec3 positive = min(albedo / 0.16, (vec3(1.0) - albedo) / 0.84);
  vec3 negative = min(albedo / 0.84, (vec3(1.0) - albedo) / 0.16);
  return clamp(desired, -max(negative, vec3(0.0)), max(positive, vec3(0.0)));
}

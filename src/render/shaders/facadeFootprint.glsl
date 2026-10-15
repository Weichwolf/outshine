vec3 facadeBoxCoverage(vec2 fractions, vec2 at, vec2 width, float storeysPerM) {
  return vec3(facadeOpeningCoverage(fractions, at.x, at.y, width.x, width.y),
              facadePaneCoverage(fractions, at.x, at.y, width.x, width.y),
              facadeStoreyBand(fractions.y, at.y, width.y, storeysPerM));
}

vec3 facadeFootprintCoverage(vec2 fractions, vec2 at, vec2 dx, vec2 dy, float storeysPerM) {
  at = fract(at);
  vec2 width = abs(dx) + abs(dy);
  if ((dx.y == 0.0 && dy.x == 0.0) || (dx.x == 0.0 && dy.y == 0.0)) {
    return facadeBoxCoverage(fractions, at, width, storeysPerM);
  }
  vec3 result = vec3(0.0);
  for (int y = 0; y < 3; ++y) {
    for (int x = 0; x < 3; ++x) {
      vec2 offset = (vec2(x, y) - 1.0) / 3.0;
      result += facadeBoxCoverage(fractions, at + offset.x * dx + offset.y * dy, width / 3.0, storeysPerM);
    }
  }
  result = clamp(result / 9.0, 0.0, 1.0);
  result.y = min(result.y, result.x);
  result.z = min(result.z, 1.0 - result.x);
  return result;
}

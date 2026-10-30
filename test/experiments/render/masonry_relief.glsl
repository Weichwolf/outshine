#include "facadeMasonryRelief.glsl"

float masonryReliefCoverage(float at, float width, vec2 ends) {
  width = max(width, 0.0001);
  float phase = fract(at - (ends.x + ends.y) * 0.5 + 0.5);
  vec2 centred = vec2(1.0 - ends.y + ends.x, 1.0 + ends.y - ends.x) * 0.5;
  if (ends.y > ends.x && phase - width * 0.5 >= centred.x &&
      phase + width * 0.5 <= centred.y) { return 1.0; }
  return periodicBand(phase, centred.x, centred.y, width);
}

float masonryReliefWall(vec2 at, vec2 width, vec2 fractions, float perM) {
  vec2 low = (1.0 - fractions) * 0.5;
  vec2 high = (1.0 + fractions) * 0.5;
  float opening = 0.0;
  if (min(fractions.x, fractions.y) > 0.0) {
    opening = masonryReliefCoverage(at.x, width.x, vec2(low.x, high.x)) *
              masonryReliefCoverage(at.y, width.y, vec2(low.y, high.y));
  }
  float stripe = min(0.16 * max(perM, 0.0), 1.0 - fractions.y);
  float courses = masonryReliefCoverage(at.y, width.y, vec2(-stripe, stripe) * 0.5);
  return max(1.0 - opening - 0.35 * courses, 0.0);
}

vec2 masonryReliefFiltered(vec4 at, vec4 dx, vec4 dy, vec2 fractions, float perM,
                           float depth, int subdivisions, out uint maskCount) {
  maskCount = 0u;
  if (depth == 0.0) { return vec2(0.0); }
  int side = max(subdivisions, 1);
  vec4 width = subdivisions == 0 ? vec4(0.0) : (abs(dx) + abs(dy)) / float(side);
  vec2 repeats = vec2(4.0, 6.25);
  vec2 brickAt = fract(fma(at.zw, repeats, -floor(at.zw * repeats)));
  vec2 gradient = vec2(0.0);
  for (int y = 0; y < side; ++y) {
    for (int x = 0; x < side; ++x) {
      vec2 offset = (vec2(x, y) + 0.5) / float(side) - 0.5;
      vec2 point = brickAt + (offset.x * dx.zw + offset.y * dy.zw) * repeats;
      vec2 bump = facadeJointGradient(point, width.zw * repeats);
      if (all(equal(bump, vec2(0.0)))) { continue; }
#if RELIEF_COUNT_WORK
      ++maskCount;
#endif
      float wall = masonryReliefWall(fract(at.xy) + offset.x * dx.xy + offset.y * dy.xy,
                                    width.xy, fractions, perM);
      gradient += bump * wall;
    }
  }
  return gradient * (depth / float(side * side));
}

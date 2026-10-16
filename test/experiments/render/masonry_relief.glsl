vec2 masonryReliefRamp(float centre, float span, float width) {
  if (span == 0.0) {
    return vec2(clamp(centre / width, 0.0, 1.0),
      centre >= 0.0 && centre < width ? 1.0 / width : 0.0);
  }
  float left = centre - span * 0.5;
  float right = centre + span * 0.5;
  if (right <= 0.0) { return vec2(0.0); }
  if (left >= width) { return vec2(1.0, 0.0); }
  if (left >= 0.0 && right <= width) { return vec2(centre / width, 1.0 / width); }
  float begin = max(left, 0.0);
  float end = min(right, width);
  float length = max(end - begin, 0.0);
  float area = length * (begin + end) / (2.0 * width);
  area += max(right - max(left, width), 0.0);
  return vec2(area, length / width) / max(span, 1e-12);
}

vec2 masonryReliefSegment(float centre, float span, vec2 ends, float bevel) {
  return masonryReliefRamp(centre - ends.x + bevel * 0.5, span, bevel) -
         masonryReliefRamp(centre - ends.y + bevel * 0.5, span, bevel);
}

vec2 masonryReliefBand(float coordinate, float span, vec2 ends, float bevel) {
  float phase = fract(coordinate);
  if (span < 1.0 && phase - span * 0.5 >= 0.0 && phase + span * 0.5 <= 1.0) {
    vec2 value = masonryReliefSegment(phase, span, ends, bevel);
    return vec2(clamp(value.x, 0.0, 1.0), value.y);
  }
  float tail = fract(span);
  float left = fract(phase - span * 0.5);
  float right = left + tail;
  vec2 lengths = vec2(min(right, 1.0) - left, max(right - 1.0, 0.0));
  vec2 first = masonryReliefSegment(left + lengths.x * 0.5, lengths.x, ends, bevel);
  vec2 second = masonryReliefSegment(lengths.y * 0.5, lengths.y, ends, bevel);
  vec2 value = (first * lengths.x + second * lengths.y) / max(span, 1e-12);
  value.x += (ends.y - ends.x) * (span - tail) / max(span, 1e-12);
  return vec2(clamp(value.x, 0.0, 1.0), value.y);
}

vec2 masonryReliefGradient(vec2 at, vec2 width) {
  vec2 gradient = vec2(0.0);
  for (int row = 0; row < 2; ++row) {
    float shift = float(row) * 0.5;
    vec2 horizontal = masonryReliefBand(at.x - shift, width.x, vec2(0.02, 0.98), 0.002 * 4.0);
    vec2 vertical = masonryReliefBand(at.y, width.y,
        vec2(0.03125, 0.46875) + shift, 0.002 * 6.25);
    gradient += vec2(horizontal.y * vertical.x, horizontal.x * vertical.y);
  }
  return gradient * vec2(4.0, 6.25);
}

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
      vec2 bump = masonryReliefGradient(point, width.zw * repeats);
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

#ifndef OUTSHINE_FACADE_MASONRY_RELIEF
#define OUTSHINE_FACADE_MASONRY_RELIEF

vec2 facadeJointRamp(float centre, float span, float width) {
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

vec2 facadeJointSegment(float centre, float span, vec2 ends, float bevel) {
  return facadeJointRamp(centre - ends.x + bevel * 0.5, span, bevel) -
         facadeJointRamp(centre - ends.y + bevel * 0.5, span, bevel);
}

vec2 facadeJointBand(float coordinate, float span, vec2 ends, float bevel) {
  float phase = fract(coordinate);
  if (span < 1.0 && phase - span * 0.5 >= 0.0 && phase + span * 0.5 <= 1.0) {
    vec2 value = facadeJointSegment(phase, span, ends, bevel);
    return vec2(clamp(value.x, 0.0, 1.0), value.y);
  }
  float tail = fract(span);
  float left = fract(phase - span * 0.5);
  float right = left + tail;
  vec2 lengths = vec2(min(right, 1.0) - left, max(right - 1.0, 0.0));
  vec2 first = facadeJointSegment(left + lengths.x * 0.5, lengths.x, ends, bevel);
  vec2 second = facadeJointSegment(lengths.y * 0.5, lengths.y, ends, bevel);
  vec2 value = (first * lengths.x + second * lengths.y) / max(span, 1e-12);
  value.x += (ends.y - ends.x) * (span - tail) / max(span, 1e-12);
  return vec2(clamp(value.x, 0.0, 1.0), value.y);
}

float facadeJointOverlap(float at, float span, float width, float centre) {
  float distance = fract(at) - centre;
  distance -= floor(distance + 0.5);
  return max(min(min(span, width), (span + width) * 0.5 - abs(distance)), 0.0);
}

float facadeJointSlope(float at, float span, vec2 ends, float bevel) {
  if (span == 0.0) { return facadeJointSegment(fract(at), 0.0, ends, bevel).y; }
  float tail = fract(span);
  if (tail == 0.0) { return 0.0; }
  float centre = at - floor(span) * 0.5;
  float rising;
  float falling;
  if (tail + bevel <= 1.0) {
    rising = facadeJointOverlap(centre, tail, bevel, ends.x);
    falling = facadeJointOverlap(centre, tail, bevel, ends.y);
  } else {
    rising = bevel - facadeJointOverlap(centre + 0.5, 1.0 - tail, bevel, ends.x);
    falling = bevel - facadeJointOverlap(centre + 0.5, 1.0 - tail, bevel, ends.y);
  }
  return (rising - falling) / (max(span, 1e-12) * bevel);
}

vec2 facadeJointGradient(vec2 at, vec2 width) {
  vec2 gradient = vec2(0.0);
  for (int row = 0; row < 2; ++row) {
    float shift = float(row) * 0.5;
    float horizontal = facadeJointSlope(at.x - shift, width.x, vec2(0.02, 0.98), 0.002 * 4.0);
    float vertical = facadeJointSlope(at.y, width.y,
        vec2(0.03125, 0.46875) + shift, 0.002 * 6.25);
    if (horizontal != 0.0) {
      gradient.x += horizontal * facadeJointBand(at.y, width.y,
          vec2(0.03125, 0.46875) + shift, 0.002 * 6.25).x;
    }
    if (vertical != 0.0) {
      gradient.y += vertical * facadeJointBand(at.x - shift, width.x,
          vec2(0.02, 0.98), 0.002 * 4.0).x;
    }
  }
  return gradient * vec2(4.0, 6.25);
}

#endif

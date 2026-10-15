int facadeClip(inout vec2 polygon[8], int count, vec2 direction, float offset) {
  vec2 result[8];
  int size = 0;
  if (count == 0) { return 0; }
  vec2 previous = polygon[count - 1];
  float before = dot(previous, direction) + offset;
  for (int i = 0; i < count; ++i) {
    vec2 current = polygon[i];
    float after = dot(current, direction) + offset;
    if ((before >= 0.0) != (after >= 0.0)) {
      result[size++] = mix(previous, current, before / (before - after));
    }
    if (after >= 0.0) { result[size++] = current; }
    previous = current;
    before = after;
  }
  for (int i = 0; i < size; ++i) { polygon[i] = result[i]; }
  return size;
}

float facadeClippedRectangle(vec2 at, vec2 dx, vec2 dy, vec2 low, vec2 high) {
  vec2 polygon[8];
  polygon[0] = vec2(-0.5, -0.5);
  polygon[1] = vec2(0.5, -0.5);
  polygon[2] = vec2(0.5, 0.5);
  polygon[3] = vec2(-0.5, 0.5);
  int count = 4;
  count = facadeClip(polygon, count, vec2(dx.x, dy.x), at.x - low.x);
  count = facadeClip(polygon, count, -vec2(dx.x, dy.x), high.x - at.x);
  count = facadeClip(polygon, count, vec2(dx.y, dy.y), at.y - low.y);
  count = facadeClip(polygon, count, -vec2(dx.y, dy.y), high.y - at.y);
  if (count == 0) { return 0.0; }
  float area = 0.0;
  vec2 previous = polygon[count - 1];
  for (int i = 0; i < count; ++i) {
    area += previous.x * polygon[i].y - previous.y * polygon[i].x;
    previous = polygon[i];
  }
  return 0.5 * abs(area);
}

float facadeClippedPeriodic(vec2 at, vec2 dx, vec2 dy, vec2 low, vec2 high) {
  at = fract(at);
  vec2 width = abs(dx) + abs(dy);
  float box = periodicBand(at.x, low.x, high.x, width.x) * periodicBand(at.y, low.y, high.y, width.y);
  float size = max(width.x, width.y);
  if (size >= 2.0 || min(width.x, width.y) < 0.0001 || box == 0.0 || box == 1.0) { return box; }
  if ((dx.y == 0.0 && dy.x == 0.0) || (dx.x == 0.0 && dy.y == 0.0)) { return box; }
  ivec2 begin = ivec2(floor(at - width * 0.5));
  ivec2 end = ivec2(floor(at + width * 0.5));
  float area = 0.0;
  for (int y = begin.y; y <= end.y; ++y) {
    for (int x = begin.x; x <= end.x; ++x) {
      area += facadeClippedRectangle(at, dx, dy, vec2(x, y) + low, vec2(x, y) + high);
    }
  }
  return mix(clamp(area, 0.0, 1.0), box, smoothstep(1.0, 2.0, size));
}

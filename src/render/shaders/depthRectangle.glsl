bool depthRectangleOccluded(vec2 lo, vec2 hi, float nearest, vec2 source,
                              uvec4 widths, uvec4 heights, uvec4 offsets) {
  float across = 0.5 * max((hi.x - lo.x) * source.x, (hi.y - lo.y) * source.y);
  uint level = 0u;
  while (level + 1u < 4u && across > 2.0) { across *= 0.5; level += 1u; }
  const uint wide = widths[level], high = heights[level], first = offsets[level];
  const vec2 blocks = source / float(2u << level);
  const uvec2 from = uvec2(min(uint(lo.x * blocks.x), wide - 1u), min(uint(lo.y * blocks.y), high - 1u));
  const uvec2 to = uvec2(min(uint(hi.x * blocks.x), wide - 1u), min(uint(hi.y * blocks.y), high - 1u));
  if (to.x - from.x >= 4u || to.y - from.y >= 4u) { return false; }
  float farthest = 1.0;
  for (uint y = from.y; y <= to.y && y < from.y + 4u; ++y) {
    for (uint x = from.x; x <= to.x && x < from.x + 4u; ++x) {
      farthest = min(farthest, pyramid[first + y * wide + x]);
    }
  }
  return nearest < farthest;
}

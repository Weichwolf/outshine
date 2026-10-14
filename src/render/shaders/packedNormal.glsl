vec3 unpackedNormal(uint word) {
  vec2 folded = vec2(word >> 16u, word & 0xffffu) / 65535.0 * 2.0 - 1.0;
  vec3 unit = vec3(folded, 1.0 - abs(folded.x) - abs(folded.y));
  if (unit.z < 0.0) {
    unit.xy = (1.0 - abs(unit.yx)) * mix(vec2(-1.0), vec2(1.0), greaterThanEqual(unit.xy, vec2(0.0)));
  }
  return normalize(unit);
}

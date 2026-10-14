float substrateFiltered(vec2 atM, float footprintM, float wavelengthM) {
  float visibility = 1.0 - smoothstep(0.25, 0.8, footprintM / wavelengthM);
  return visibility > 0.0 ? (surfaceNoise2(atM / wavelengthM) - 0.5) * visibility : 0.0;
}

vec2 substrateGrains(vec2 atM, float footprintM, float grainM, float heightM) {
  if (grainM <= 0.0 || heightM <= 0.0 || footprintM >= grainM * 0.8) { return vec2(1.0, 0.0); }
  float visibility = 1.0 - smoothstep(0.25, 0.8, footprintM / grainM);
  vec2 point = atM / grainM;
  ivec2 cell = ivec2(floor(point));
  float nearest = 1.0e30;
  float tint = 0.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      ivec2 site = cell + ivec2(x, y);
      uint bits = surfaceHash(ivec3(site, 0));
      vec2 jitter = 0.25 + 0.5 * vec2(bits & 255u, (bits >> 8u) & 255u) / 255.0;
      vec2 offset = vec2(site) + jitter - point;
      float distance = dot(offset, offset);
      bool nearer = distance < nearest;
      tint = nearer ? float((bits >> 16u) & 255u) / 255.0 - 0.5 : tint;
      nearest = min(nearest, distance);
    }
  }
  float relief = max(1.0 - sqrt(nearest), 0.0);
  return vec2(1.0 + 0.35 * visibility * tint, heightM * visibility * relief * relief);
}

GroundDetail substrateDetail(int which, vec2 atM, float footprintM, uint rows) {
  uint row = groundSubstrateRow(which, rows);
  uint at = 4u * (rows + 2u) + rows + 1u + row * 8u;
  float coarseM = groundPalette[at + 2u];
  float fineM = groundPalette[at + 3u];
  float grainM = groundPalette[at];
  float largestM = max(max(coarseM, fineM), grainM);
  if (coarseM <= 0.0 || fineM <= 0.0 || footprintM >= largestM * 0.8) {
    return GroundDetail(1.0, 0.0, 0.0);
  }
  float heightM = groundPalette[at + 1u];
  float contrast = clamp(groundPalette[at + 4u], 0.0, 1.0);
  float coarse = substrateFiltered(atM, footprintM, coarseM);
  float fine = substrateFiltered(atM, footprintM, fineM);
  vec2 grain = substrateGrains(atM, footprintM, grainM, heightM);
  return GroundDetail((1.0 + contrast * (0.7 * coarse + 0.3 * fine)) * grain.x,
                      0.06 * coarse + 0.04 * fine, heightM * fine + grain.y);
}

vec3 substrateFiltered(vec2 atM, float footprintM, float wavelengthM) {
  if (wavelengthM <= 0.0) { return vec3(0.0); }
  float visibility = 1.0 - smoothstep(0.25, 0.8, footprintM / wavelengthM);
  if (visibility <= 0.0) { return vec3(0.0); }
  vec3 field = surfaceNoise2(atM / wavelengthM);
  return vec3(field.xy / wavelengthM, field.z - 0.5) * visibility;
}

vec3 substrateGrains(vec2 atM, float footprintM, float grainM, float heightM) {
  if (grainM <= 0.0 || heightM <= 0.0 || footprintM >= grainM * 0.8) { return vec3(1.0, 0.0, 0.0); }
  float visibility = 1.0 - smoothstep(0.25, 0.8, footprintM / grainM);
  vec2 point = atM / grainM;
  ivec2 cell = ivec2(floor(point));
  vec2 fraction = fract(point);
  float nearest = 1.0e30;
  vec2 nearestOffset = vec2(0.0);
  float tint = 0.0;
  for (int y = -1; y <= 1; ++y) {
    for (int x = -1; x <= 1; ++x) {
      ivec2 site = cell + ivec2(x, y);
      uint bits = surfaceHash(ivec3(site, 0));
      vec2 jitter = 0.25 + 0.5 * vec2(bits & 255u, (bits >> 8u) & 255u) / 255.0;
      vec2 offset = vec2(x, y) + jitter - fraction;
      float distance = dot(offset, offset);
      bool nearer = distance < nearest;
      tint = nearer ? float((bits >> 16u) & 255u) / 255.0 - 0.5 : tint;
      nearestOffset = nearer ? offset : nearestOffset;
      nearest = min(nearest, distance);
    }
  }
  float radius = sqrt(nearest);
  float relief = max(1.0 - radius, 0.0);
  float slope = radius > 0.0 ? 2.0 * heightM * visibility * relief / (grainM * radius) : 0.0;
  return vec3(1.0 + 0.35 * visibility * tint,
              nearestOffset * slope);
}

GroundDetail substrateDetail(int which, vec2 atM, vec2 worldDx, vec2 worldDy, float footprintM, uint rows) {
  uint row = groundSubstrateRow(which, rows);
  uint at = 4u * (rows + 2u) + rows + 1u + row * 8u;
  float coarseM = groundPalette[at + 2u];
  float fineM = groundPalette[at + 3u];
  float grainM = groundPalette[at];
  float largestM = max(max(coarseM, fineM), grainM);
  if (coarseM <= 0.0 || fineM <= 0.0 || footprintM >= largestM * 0.8) {
    return GroundDetail(1.0, 0.0, vec2(0.0));
  }
  float heightM = groundPalette[at + 1u];
  float contrast = clamp(groundPalette[at + 4u], 0.0, 1.0);
  float coarse = substrateFiltered(atM, footprintM, coarseM).z;
  vec3 fine = substrateFiltered(atM, footprintM, fineM);
  vec3 grain = substrateGrains(atM, footprintM, grainM, heightM);
  vec2 gradient = heightM * fine.xy + grain.yz;
  return GroundDetail((1.0 + contrast * (0.7 * coarse + 0.3 * fine.z)) * grain.x,
                      0.06 * coarse + 0.04 * fine.z,
                       vec2(dot(gradient, worldDx), dot(gradient, worldDy)));
}

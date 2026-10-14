struct GroundDetail {
  float AlbedoScale;
  float RoughnessOffset;
  float BumpM;
  vec2 BumpDxDy;
};

uint groundMaterialRow(int which, uint rows) {
  return which >= 0 && uint(which) < rows ? uint(which) : rows;
}

uint groundSubstrateRow(int which, uint rows) {
  uint row = groundMaterialRow(which, rows);
  if (row < rows) { return row; }
  int unmapped = int(groundClasses[2]);
  return unmapped >= 0 && uint(unmapped) < rows ? uint(unmapped) : rows;
}

vec4 groundWears(int which, uint rows) {
  uint row = groundMaterialRow(which, rows);
  return vec4(groundPalette[4u + row * 4u + 0u],
              groundPalette[4u + row * 4u + 1u],
              groundPalette[4u + row * 4u + 2u],
              groundPalette[4u + row * 4u + 3u]);
}

float groundRockWeight(int which, uint rows, float slopeDeg) {
  uint row = groundSubstrateRow(which, rows);
  float limitDeg = groundPalette[4u * (rows + 2u) + row];
  float bandDeg = groundPalette[2];
  uint rock = floatBitsToUint(groundPalette[1]);
  return row == rock ? 1.0 : smoothstep(limitDeg, limitDeg + bandDeg, slopeDeg);
}

vec4 groundWearsSlope(int which, uint rows, float slopeDeg) {
  int rock = int(floatBitsToUint(groundPalette[1]));
  return mix(groundWears(which, rows), groundWears(rock, rows),
             groundRockWeight(which, rows, slopeDeg));
}

layout(location = 0) in vec3 grid;
layout(location = 1) in uint patchCode;
layout(std430, set = 0, binding = 1) readonly buffer Buffer_groundTiles { vec4 groundTiles[]; };
layout(std430, set = 0, binding = 2) readonly buffer Buffer_groundFractions { float groundFractions[]; };
vec4 c0, c1, c2, c3, north, south, pageInfo;

vec3 groundPatchGrid() {
  const uint id = patchCode & ~(kGroundUniformBit | kGroundLodMask);
  const uint tile = (id >> kGroundPatchBits) * 7u;
  c0 = groundTiles[tile];
  c1 = groundTiles[tile + 1u];
  c2 = groundTiles[tile + 2u];
  c3 = groundTiles[tile + 3u];
  north = groundTiles[tile + 4u];
  south = groundTiles[tile + 5u];
  pageInfo = groundTiles[tile + 6u];
  const uint part = id & ((1u << kGroundPatchBits) - 1u);
  const uint columns = uint(kGroundSide - 1) / uint(kGroundPatchCells);
  const uvec2 origin = uvec2(part % columns, part / columns) * uint(kGroundPatchCells);
  const uint fractions = (patchCode & kGroundUniformBit) != 0u ? uint(kGroundSide) : 0u;
  return vec3(groundFractions[fractions + origin.x + uint(grid.x)],
                groundFractions[fractions + origin.y + uint(grid.y)], 0.0);
}

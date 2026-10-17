#define GROUND_INT(name, value) const int name = value;
#define GROUND_UINT(name, value) const uint name = value;
#define GROUND_FLOAT(name, value) const float name = value;
#include "render/stages/GroundConstants.inc"
#undef GROUND_INT
#undef GROUND_UINT
#undef GROUND_FLOAT
layout(set = 0, binding = 0) uniform sampler2DArray heightPages;
layout(std430, set = 0, binding = 1) readonly buffer Buffer_groundTiles { vec4 groundTiles[]; };

float fineGroundHeight(uint page, ivec2 node) {
  const uint side = uint(kGroundSide + 2);
  const uint perLayer = kGroundPageColumns * kGroundPageColumns;
  const uvec2 offset = uvec2(page % kGroundPageColumns, (page % perLayer) / kGroundPageColumns) * side;
  const ivec2 at = clamp(node + 1, ivec2(0), ivec2(int(side) - 1));
  return texelFetch(heightPages, ivec3(ivec2(offset) + at, int(page / perLayer)), 0).x;
}

vec3 fineGroundNodeNormal(uint page, ivec2 node, vec2 stepM, mat3 basis) {
  const float de = fineGroundHeight(page, node + ivec2(1, 0)) - fineGroundHeight(page, node - ivec2(1, 0));
  const float dn = fineGroundHeight(page, node - ivec2(0, 1)) - fineGroundHeight(page, node + ivec2(0, 1));
  return normalize(basis * normalize(vec3(-vec2(de, dn) / (2.0 * stepM), 1.0)));
}

vec3 groundFineNormal() {
  if ((groundSampleCode & kGroundLodMask) == 0u) { return normal; }
  const uint code = groundSampleCode & ~(kGroundUniformBit | kGroundLodMask);
  const uint tile = (code >> kGroundPatchBits) * 7u;
  const mat3 basis = mat3(groundTiles[tile].xyz, groundTiles[tile + 1u].xyz, groundTiles[tile + 2u].xyz);
  const vec4 info = groundTiles[tile + 6u];
  const uint page = uint(info.x);
  const vec2 stepM = max(info.zw, vec2(1.0e-3));
  const vec2 at = clamp(groundSampleUV * float(kGroundSide - 1), vec2(0.0), vec2(float(kGroundSide - 1)));
  const ivec2 cell = min(ivec2(floor(at)), ivec2(kGroundSide - 2));
  const vec2 uv = at - vec2(cell);
  const vec3 a = fineGroundNodeNormal(page, cell, stepM, basis);
  const vec3 c = fineGroundNodeNormal(page, cell + 1, stepM, basis);
  if (uv.x >= uv.y) {
    const vec3 b = fineGroundNodeNormal(page, cell + ivec2(1, 0), stepM, basis);
    return (1.0 - uv.x) * a + (uv.x - uv.y) * b + uv.y * c;
  }
  const vec3 d = fineGroundNodeNormal(page, cell + ivec2(0, 1), stepM, basis);
  return (1.0 - uv.y) * a + (uv.y - uv.x) * d + uv.x * c;
}

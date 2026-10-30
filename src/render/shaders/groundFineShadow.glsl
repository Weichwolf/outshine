#include "geometryView.glsl"

vec3 fineGroundPoint(uint page, ivec2 node, vec4 north, vec4 south, float sagInv) {
  const vec2 uv = vec2(node) / float(kGroundSide - 1);
  const vec2 en = mix(mix(north.xy, north.zw, uv.x), mix(south.xy, south.zw, uv.x), uv.y);
  return vec3(en, fineGroundHeight(page, node) - 0.5 * dot(en, en) * sagInv);
}

vec4 fineGroundShadow(vec4 coarse, out vec2 receiverSlope) {
  receiverSlope = vec2(0.0);
  if ((groundSampleCode & kGroundLodMask) == 0u) { return coarse; }
  const uint code = groundSampleCode & ~(kGroundUniformBit | kGroundLodMask);
  const uint tile = (code >> kGroundPatchBits) * 7u;
  const vec4 info = groundTiles[tile + 6u];
  const uint page = uint(info.x);
  const vec2 at = clamp(groundSampleUV, vec2(0.0), vec2(1.0)) * float(kGroundSide - 1);
  const ivec2 cell = min(ivec2(floor(at)), ivec2(kGroundSide - 2));
  const vec2 uv = at - vec2(cell);
  const vec4 north = groundTiles[tile + 4u], south = groundTiles[tile + 5u];
  const vec3 a = fineGroundPoint(page, cell, north, south, info.y);
  const vec3 c = fineGroundPoint(page, cell + 1, north, south, info.y);
  vec3 local;
  vec3 second;
  if (uv.x >= uv.y) {
    const vec3 b = fineGroundPoint(page, cell + ivec2(1, 0), north, south, info.y);
    second = b;
    local = (1.0 - uv.x) * a + (uv.x - uv.y) * b + uv.y * c;
  } else {
    const vec3 e = fineGroundPoint(page, cell + ivec2(0, 1), north, south, info.y);
    second = e;
    local = (1.0 - uv.y) * a + (uv.y - uv.x) * e + uv.x * c;
  }
  const mat4 model = mat4(groundTiles[tile],
                          groundTiles[tile + 1u],
                          groundTiles[tile + 2u],
                          groundTiles[tile + 3u] + vec4(s.shift.xyz, 0.0));
  vec4 projected = s.lightFromWorld * (model * vec4(local, 1.0));
  vec4 dx = s.lightFromWorld * (model * vec4(second - a, 0.0));
  vec4 dy = s.lightFromWorld * (model * vec4(c - a, 0.0));
  receiverSlope = shadowReceiverPlaneSlope(projected, dx, dy);
  return projected;
}

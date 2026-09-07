#define GROUND_INT(name, value) const int name = value;
#define GROUND_UINT(name, value) const uint name = value;
#define GROUND_FLOAT(name, value) const float name = value;
#include "../stages/GroundConstants.inc"
#undef GROUND_INT
#undef GROUND_UINT
#undef GROUND_FLOAT
layout(location = 0) in vec3 grid;
layout(location = 1) in vec4 c0;
layout(location = 2) in vec4 c1;
layout(location = 3) in vec4 c2;
layout(location = 4) in vec4 c3;
layout(location = 5) in vec4 north;
layout(location = 6) in vec4 south;
layout(location = 7) in vec4 pageInfo;
const int kGroundPageSide = kGroundSide + 2;
const uint kGroundPagesPerLayer = kGroundPageColumns * kGroundPageColumns;

float groundNodeAt(sampler2DArray pages, uint page, int i, int j) {
  const ivec2 c = clamp(ivec2(i, j) + 1, ivec2(0), ivec2(kGroundPageSide - 1));
  const uvec2 offset = uvec2(page % kGroundPageColumns,
                             (page % kGroundPagesPerLayer) / kGroundPageColumns) * uint(kGroundPageSide);
  return texelFetch(pages, ivec3(uvec2(c) + offset, page / kGroundPagesPerLayer), 0).x;
}

struct GroundPoint {
  vec3 local;
  vec3 rim;
  vec3 normal;
};

GroundPoint groundPointAt(sampler2DArray pages) {
  const float u = grid.x;
  const float w = grid.y;
  const uint page = uint(pageInfo.x);
  const float sagInv = pageInfo.y;
  const float stepE = max(pageInfo.z, 1.0e-3);
  const float stepN = max(pageInfo.w, 1.0e-3);
  const int i = int(round(u * float(kGroundSide - 1)));
  const int j = int(round(w * float(kGroundSide - 1)));
  const float h = groundNodeAt(pages, page, i, j);
  const vec2 en = mix(mix(north.xy, north.zw, u), mix(south.xy, south.zw, u), w);
  const float sag = 0.5 * dot(en, en) * sagInv;
  const float skirt = grid.z * kGroundSkirtSteps * max(stepE, stepN);
  GroundPoint p;
  p.local = vec3(en.x, en.y, h - sag - skirt);
  p.rim = vec3(en.x, en.y, h - sag);
  const float dhE =
      (groundNodeAt(pages, page, i + 1, j) - groundNodeAt(pages, page, i - 1, j)) / (2.0 * stepE);
  const float dhN =
      (groundNodeAt(pages, page, i, j - 1) - groundNodeAt(pages, page, i, j + 1)) / (2.0 * stepN);
  p.normal = normalize(vec3(-dhE, -dhN, 1.0));
  return p;
}

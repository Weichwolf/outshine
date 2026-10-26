#version 450
#extension GL_GOOGLE_include_directive : require
#include "patch.glsl"
layout(location = 0) out float depthBound;

void main() {
  if (!validPatch()) { discard; }
  int scale = int(fixedPatch.meta.w);
  ivec2 p = ivec2(gl_FragCoord.xy) * scale;
  for (uint i = 0u; i < fixedPatch.meta.x; ++i) {
    ivec2 a = fixedPatch.corners[i].xy;
    ivec2 d = fixedPatch.corners[(i + 1u) % fixedPatch.meta.x].xy - a;
    int least = d.x * (p.y - a.y) - d.y * (p.x - a.x) +
                min(0, d.x * scale) + min(0, -d.y * scale);
    if (least < 0) { discard; }
  }
  depthBound = fixedPatch.depth.x;
  gl_FragDepth = fixedPatch.depth.x;
}

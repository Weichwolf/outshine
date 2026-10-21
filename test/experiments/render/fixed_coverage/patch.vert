#version 450
#extension GL_GOOGLE_include_directive : require
#include "patch.glsl"

void main() {
  if (!validPatch()) {
    gl_Position = vec4(2.0, 0.0, 0.0, 1.0);
    return;
  }
  uint triangle = uint(gl_VertexIndex) / 3u;
  uint corner = uint(gl_VertexIndex) % 3u;
  uint index = corner == 0u ? 0u : triangle + corner;
  if (index >= fixedPatch.meta.x) {
    gl_Position = vec4(2.0, 0.0, 0.0, 1.0);
    return;
  }
  vec2 uv = vec2(fixedPatch.corners[index].xy) / vec2(fixedPatch.meta.yz * fixedPatch.meta.w);
  gl_Position = vec4(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0, fixedPatch.depth.x, 1.0);
}

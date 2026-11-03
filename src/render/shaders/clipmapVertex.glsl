#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING out
#define SURFACE_VERTEX_STAGE 1
#define GROUND_CLIPMAPS 1
#include "surfaceVaryings.glsl"
#include "geometryView.glsl"
#include "clipmapVertexCore.glsl"

void main() {
  clipmapLevel = uint(gl_InstanceIndex);
  ClipmapRing ring = clipmapRings[clipmapLevel];
  clipmapGrid = grid;
  ClipmapPoint point = clipmapPoint(clipmapLevel);
  position = point.Position;
  gl_Position = s.viewProj * vec4(position, 1.0);
  groundWorldM = point.World;
  uv = point.World.xz;
  uv1 = vec2(0.0);
  colour = vec4(1.0);
  normal = vec3(0.0, 1.0, 0.0);
  localPosition = groundWorldM;
  lightSpace = s.lightFromWorld * vec4(position, 1.0);
#if SURFACE_WRITES_VELOCITY
  curClip = gl_Position;
  vec2 previous = ring.RelativeCentres.zw + grid * ring.CentreStep.z;
  float previousHeight = clipmapSurface(grid, clipmapLevel, ring.PreviousMorph);
  prevClip = s.prevViewProj * vec4(previous.x, previousHeight + s.prevShift.y, -previous.y, 1.0);
#endif
  writeSurfaceVaryings();
}

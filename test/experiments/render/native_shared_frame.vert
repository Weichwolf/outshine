#version 450
#extension GL_GOOGLE_include_directive : require
#include "subjectPlacement.glsl"
#include "subjectView.glsl"
struct Point { vec4 p; vec4 shift; };
layout(std430,set=0,binding=1) readonly buffer Inputs { Point points[]; };
layout(location=0) flat out vec4 captured;
void main() {
  uint id=gl_InstanceIndex;
  vec3 p=points[id].p.xyz;
  vec4 pointShift=points[id].shift;
  GpuPlacement placement=placements[id];
  mat4 m = placement.current;
  vec4 stable = m * vec4(p, 1.0);
  vec4 world = stable + vec4(pointShift.xyz, 0.0);
  captured = s.viewProj * world;

  const vec2 corners[6]=vec2[6](vec2(0,0),vec2(1,0),vec2(0,1),vec2(1,0),vec2(1,1),vec2(0,1));
  vec2 corner=corners[gl_VertexIndex];
  gl_Position=vec4(2.0*(float(id)+corner.x)/float(points.length())-1.0,2.0*corner.y-1.0,0.5,1.0);
}

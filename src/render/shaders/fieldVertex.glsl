#version 450
#extension GL_GOOGLE_include_directive : require
#include "subjectView.glsl"
#include "subjectPlacement.glsl"
#include "fieldInput.glsl"
#define VARYING out
#define SUBJECT_VERTEX_STAGE 1
#define SUBJECT_CONSTANT_UV 1
#include "subjectVaryings.glsl"
layout(location = 11) flat out uvec4 fieldFace;
layout(location = 12) flat out float fieldRoofLevel;

void main() {
  GpuPlacement placement = placements[gl_InstanceIndex];
  mat4 model = placement.current;
  vec4 stable = model * vec4(fieldPosition, 1.0);
  vec4 world = stable + vec4(s.shift.xyz, 0.0);
  gl_Position = s.viewProj * world;
  position = world.xyz;
  groundWorldM = vec3(stable.x, stable.y, -stable.z);
  localPosition = fieldPosition;
  lightSpace = s.lightFromWorld * world;
  normal = normalize(model[0].xyz * fieldNormal.x + model[1].xyz * fieldNormal.y +
                     model[2].xyz * fieldNormal.z);
  colour = vec4(1.0);
  fieldFace = fieldDescriptor();
  fieldRoofLevel = fieldLevelReserved.x;
#if SUBJECT_WRITES_VELOCITY
  curClip = gl_Position;
  mat4 previousPlacement = placement.previous;
  previousPlacement[3] += vec4(s.prevShift.xyz, 0.0);
  prevClip = s.prevViewProj * (previousPlacement * vec4(fieldPosition, 1.0));
#endif
  writeSubjectVaryings();
}

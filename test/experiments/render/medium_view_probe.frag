#version 450
#extension GL_GOOGLE_include_directive : require
#include "mediumView.glsl"

layout(location = 0) out vec4 geometry;
layout(location = 1) out vec4 origin;
layout(location = 2) out vec4 direction;

layout(std140, set = 3, binding = 2) uniform MediumFrame {
  MediumView View;
}

frame;

void main() {
  MediumViewRay ray = mediumViewRay(frame.View, gl_FragCoord.xyz);
  geometry = vec4(ray.Air.RadiusKm, ray.Air.CosView, ray.Air.CosSun, ray.Air.DistanceKm);
  origin = vec4(ray.OriginOffsetM, ray.Air.ViewSun);
  direction = vec4(ray.Direction, gl_FragCoord.z);
}

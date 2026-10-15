#ifndef OUTSHINE_MEDIUM_VIEW_GLSL
#define OUTSHINE_MEDIUM_VIEW_GLSL

#include "airRay.glsl"

struct MediumView {
  vec4 Right;
  vec4 Up;
  vec4 Fwd;
  vec4 WorldUp;
  vec4 SunDir;
  vec2 TanHalf;
  float Illuminance;
  float EyeRadiusKm;
  vec4 DepthReconstruction;
  Medium Air;
  vec4 PixelToNdc;
  vec4 OrthoProjection;
};

struct MediumViewRay {
  vec3 OriginOffsetM;
  vec3 Direction;
  AirRay Air;
};

MediumViewRay mediumViewRay(MediumView frame, vec3 fragment) {
  vec2 ndc = fragment.xy * frame.PixelToNdc.xy + frame.PixelToNdc.zw;
  bool ortho = frame.OrthoProjection.z > 0.0;
  vec3 offsetM = ortho ? frame.Right.xyz * (ndc.x * frame.OrthoProjection.x) +
                             frame.Up.xyz * (ndc.y * frame.OrthoProjection.y)
                       : vec3(0.0);
  vec3 direction = ortho ? normalize(frame.Fwd.xyz)
                         : normalize(frame.Fwd.xyz + frame.Right.xyz * (ndc.x * frame.TanHalf.x) +
                                     frame.Up.xyz * (ndc.y * frame.TanHalf.y));
  vec4 depth = frame.DepthReconstruction;
  float delta = fragment.z - frame.OrthoProjection.w;
  float alongViewM = (depth.x - delta * depth.y) / (depth.z + delta * depth.w);
  float distanceKm = max(0.0, alongViewM) * 0.001 / (ortho ? 1.0 : dot(direction, frame.Fwd.xyz));
  vec3 offsetKm = offsetM * 0.001;
  float deltaSquared = dot(offsetKm, offsetKm);
  float radialOffset = dot(frame.WorldUp.xyz, offsetKm);
  float raised = 2.0 * frame.EyeRadiusKm * radialOffset + deltaSquared;
  float sphereRadiusKm = sqrt(frame.EyeRadiusKm * frame.EyeRadiusKm + raised);
  float radiusKm = frame.EyeRadiusKm + raised / (sphereRadiusKm + frame.EyeRadiusKm);
  vec3 positionKm = frame.WorldUp.xyz * frame.EyeRadiusKm + offsetKm;
  AirRay ray = AirRay(radiusKm,
                      clamp(dot(direction, positionKm) / radiusKm, -1.0, 1.0),
                      clamp(dot(frame.SunDir.xyz, positionKm) / radiusKm, -1.0, 1.0),
                      clamp(dot(direction, frame.SunDir.xyz), -1.0, 1.0),
                      distanceKm);
  return MediumViewRay(offsetM, direction, ray);
}

#endif

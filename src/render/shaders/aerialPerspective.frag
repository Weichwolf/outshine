#version 450
#extension GL_GOOGLE_include_directive : require
#include "airPrefix.glsl"
layout(location = 0) in vec2 ndc;
layout(location = 0) out vec4 colour;
layout(set = 2, binding = 0) uniform sampler2D scene;
layout(set = 2, binding = 1) uniform sampler2D depth;
layout(set = 2, binding = 2) uniform sampler2D multiScatter;
layout(set = 2, binding = 3) uniform sampler2D veil;
layout(std140, set = 3, binding = 0) uniform Pushed {
  vec4 right;
  vec4 up;
  vec4 fwd;
  vec4 worldUp;
  vec4 sunDir;
  vec2 tanHalf;
  float illuminance;
  float eyeRadiusKm;
  vec4 depthReconstruction;
  Medium air;
} pushed;
vec4 shaded() {

  uvec2 px = uvec2(gl_FragCoord.xy);
  vec4 lit = texelFetch(scene, ivec2(px), 0);
  float written = texelFetch(depth, ivec2(px), 0).r;

  if (written <= 0.0) { return lit; }

  vec3 dir = normalize(pushed.fwd.xyz + ndc.x * pushed.tanHalf.x * pushed.right.xyz +
                         ndc.y * pushed.tanHalf.y * pushed.up.xyz);
  vec4 reconstruction = pushed.depthReconstruction;
  float alongView = (reconstruction.x - written * reconstruction.y) /
                    max(reconstruction.z + written * reconstruction.w, 1.0e-9);
  float reachKm = alongView * 0.001 / max(1.0e-6, dot(dir, normalize(pushed.fwd.xyz)));
  if (reachKm <= 0.0) { return lit; }

  vec3 worldUp = normalize(pushed.worldUp.xyz);
  vec3 sunDir = normalize(pushed.sunDir.xyz);
  AirRay ray = AirRay(pushed.eyeRadiusKm, clamp(dot(dir, worldUp), -1.0, 1.0),
      clamp(dot(sunDir, worldUp), -1.0, 1.0), clamp(dot(dir, sunDir), -1.0, 1.0), reachKm);
  AirPathSpan span = airPhysicalSpan(pushed.air, ray);
  if (span.EndKm <= span.BeginKm || reachKm <= span.BeginKm) { return lit; }
  ray = airPathRay(pushed.air, ray, span);
  AirPrefix prefix = airPrefixBegin(pushed.air, ray, veil, multiScatter);
  float queryKm = min(reachKm - span.BeginKm, ray.DistanceKm);
  AirTransfer air = airPrefixAdvance(prefix, pushed.air, queryKm, veil, multiScatter);
  return vec4(lit.rgb * air.Transmittance +
      pushed.illuminance * air.RadiancePerLux, lit.a);
}

void main() { colour = shaded(); }

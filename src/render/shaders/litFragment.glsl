#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING in
#if PRISM_GRID
#define SURFACE_FRONT_FACING true
#endif
#if LIT_TEXTURED == 0
#define SURFACE_CONSTANT_UV 1
#endif
#include "surfaceVaryings.glsl"
#if SURFACE_CONSTANT_UV || PRISM_GRID
float facadeBay = 0.0;
float facadeCode = 0.0;
#else
layout(location = 11) in float facadeBay;
layout(location = 12) flat in float facadeCode;
#endif
#include "surfaceOutput.glsl"
#include "surfaceTransfer.glsl"
#include "material.glsl"
#include "surfaceBindings.glsl"
#include "brdfTables.glsl"
const float kPi = acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "sheenLobe.glsl"
#include "iridescenceLobe.glsl"
#include "microfacetEnergy.glsl"
#if PRISM_GRID && SURFACE_BASE_LOCATION == 0
#include "prismReceiver.glsl"
vec4 prismShadowReceiver;
#endif
#if SURFACE_BASE_LOCATION == 0
#include "surfaceLighting.glsl"
#endif
#include "surfaceGradient.glsl"
#include "facadePattern.glsl"
SurfaceDifferential surfaceDx;
SurfaceDifferential surfaceDy;
#if LIT_MAPPED
#if PRISM_GRID
vec4 tangent;
#else
layout(location = 9) in vec4 tangent;
#endif
#include "normalFromMap.glsl"
#endif
#if PRISM_GRID
#include "prismSurface.glsl"
#endif
#if !PRISM_GRID
void readSurfaceDifferentials() {
  surfaceDx.uv = dFdx(uv);
  surfaceDy.uv = dFdy(uv);
  surfaceDx.uv1 = dFdx(uv1);
  surfaceDy.uv1 = dFdy(uv1);
  surfaceDx.facade = dFdx(vec2(facadeBay, uv.y));
  surfaceDy.facade = dFdy(vec2(facadeBay, uv.y));
  surfaceDx.position = dFdx(position);
  surfaceDy.position = dFdy(position);
  surfaceDx.localPosition = dFdx(localPosition);
  surfaceDy.localPosition = dFdy(localPosition);
  surfaceDx.metric = dFdx(groundWorldM);
  surfaceDy.metric = dFdy(groundWorldM);
}
#endif
#include "surfaceMaterial.glsl"
void main() {
#if PRISM_GRID
  if (!readPrismSurface()) { discard; }
#else
  readSurfaceVaryings();
  readSurfaceDifferentials();
#endif
  M surface = materialAt();
  SurfaceSample sampled = sampleSurfaceMaterial(surface);
#if SURFACE_BASE_LOCATION == 0
  SURFACE_SHADOW_RECEIVER receiverSlope = sunShadowReceiverSlope(lightSpace);
#endif
#if LIT_KIND == 1
  if (materialCoverage(surface, sampled.Alpha, colour.a) < surface.cut) { discard; }
#endif
#if SURFACE_BASE_LOCATION > 0
  outputMaterial(sampled.Albedo, sampled.Roughness, sampled.Metalness);
  outputSurface(vec4(0.0), sampled.Normal, surface.identity);
#else
  vec4 shaded = vec4(shadeRow(surface, localPosition, sampled.Normal, position, sampled.Albedo,
      sampled.Metalness, sampled.Roughness, sampled.SpecularF0, sampled.SpecularF90, sampled.Emission, sampled.Tangent, lightSpace, receiverSlope, shadowMap), 1.0);
#if LIT_KIND == 2
  shaded.a = surface.factor * sampled.Alpha * colour.a;
#endif
#if LIT_KIND == 3
  vec3 medium = surfaceMediumTransmittance(surface.attenuationColour, surface.thickness,
                                           surface.attenuationDistance);
  vec3 view = normalize(lights.viewPosition.xyz - position * lights.viewPosition.w);
  vec3 reflected = brdfFresnel(sampled.SpecularF0, sampled.SpecularF90, clamp(dot(sampled.Normal, view), 0.0, 1.0));
  vec3 transmitted = (vec3(1.0) - reflected) * (1.0 - sampled.Metalness) * surface.transmission;
  SurfaceTransfer transfer = SurfaceTransfer(shaded.rgb, sampled.Albedo * medium * transmitted);
  shaded.rgb = surfaceTransferBehind(transfer, texelFetch(behindMap, ivec2(gl_FragCoord.xy), 0).rgb);
#endif
  outputSurface(shaded, sampled.Normal, surface.identity);
#endif
}

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
vec2 coordinate(vec3 u, vec3 v, float second) {
  vec3 homogeneous = vec3(mix(uv, uv1, second), 1.0);
  return vec2(dot(u, homogeneous), dot(v, homogeneous));
}
#if LIT_TEXTURED
vec4 sampleSurface(sampler2D source, vec3 u, vec3 v, float second) {
  vec2 dx = mix(surfaceDx.uv, surfaceDx.uv1, second);
  vec2 dy = mix(surfaceDy.uv, surfaceDy.uv1, second);
  return textureGrad(source, coordinate(u, v, second),
                     vec2(dot(u.xy, dx), dot(v.xy, dx)),
                     vec2(dot(u.xy, dy), dot(v.xy, dy)));
}
#endif
void main() {
#if PRISM_GRID
  if (!readPrismSurface()) { discard; }
#else
  readSurfaceVaryings();
  readSurfaceDifferentials();
#endif
  M surface = materialAt();
  vec4 tap = vec4(1.0);
  vec4 orm = vec4(1.0);
  vec3 emission = surface.emissive;
  vec3 f0 = surface.f0;
  float f90 = surface.specularWeight;
  float roughness = surface.roughness;
  vec3 tangentDir = vec3(0.0);
  vec3 shadingNormal = facing(normal, SURFACE_FRONT_FACING);
#if LIT_TEXTURED
  tap = sampleSurface(colourMap, surface.colourUvU, surface.colourUvV, surface.colourUvSecond);
  orm = sampleSurface(metalRoughMap, surface.metalRoughUvU, surface.metalRoughUvV, surface.metalRoughUvSecond);
#if SURFACE_BASE_LOCATION == 0
  emission *= sampleSurface(emissiveMap, surface.emissiveUvU, surface.emissiveUvV, surface.emissiveUvSecond).rgb;
  float strength = sampleSurface(specularStrengthMap, surface.specularStrengthUvU, surface.specularStrengthUvV, surface.specularStrengthUvSecond).a;
  f0 *= strength * sampleSurface(specularTintMap, surface.specularTintUvU, surface.specularTintUvV, surface.specularTintUvSecond).rgb;
  f90 *= strength;
#endif
#endif
#if SURFACE_BASE_LOCATION == 0 && LIT_TEXTURED
  if (surface.scattering > 1.5) {
    orm.g = pow(clamp(orm.g / max(orm.a, 1e-8), 0.0, 1.0), 0.25);
    orm.b /= max(orm.a, 1e-8);
  }
#endif
  roughness *= orm.g;
#if LIT_MAPPED
  vec4 mapped = sampleSurface(normalMap, surface.normalUvU, surface.normalUvV, surface.normalUvSecond);
  shadingNormal = normalFromMap(normal, tangent, mapped.xyz * 2.0 - 1.0, surface.normalScale, SURFACE_FRONT_FACING);
  roughness = roughenedBy(roughness, mapped.w);
  tangentDir = tangent.xyz;
#endif
  vec3 albedo = surface.base.rgb * tap.rgb;
  float metalness = surface.metalness * orm.b;
  if (surface.pattern > 0.5) {
    vec2 heightChange;
    applyFacade(int(facadeCode), vec2(facadeBay, uv.y), colour.rgb, localPosition, groundWorldM, facing(normal, SURFACE_FRONT_FACING), surface.pattern > 1.5, uv1, surfaceDx, surfaceDy, albedo, roughness, heightChange);
    shadingNormal = bumpNormal(facing(normal, SURFACE_FRONT_FACING), shadingNormal, surfaceDx.position, surfaceDy.position, heightChange.x, heightChange.y);
  }
  else { albedo *= colour.rgb; }
#if SURFACE_BASE_LOCATION == 0
  if (surface.scattering > 0.5) {
    surface.thinReflection = albedo;
#if LIT_TEXTURED
    surface.diffuseTransmission *= sampleSurface(diffuseTransmissionMap, surface.diffuseTransmissionUvU, surface.diffuseTransmissionUvV,
                   surface.diffuseTransmissionUvSecond).rgb;
    if (surface.scattering > 1.5) {
      float coverage = max(tap.a, 1e-8);
      vec4 sheet = sampleSurface(diffuseReflectionMap, surface.diffuseReflectionUvU, surface.diffuseReflectionUvV,
                     surface.diffuseReflectionUvSecond);
      surface.thinWeight = clamp(sheet.a / coverage, 0.0, 1.0);
      surface.thinReflection = sheet.rgb / coverage;
      surface.diffuseTransmission /= coverage;
      albedo /= max(coverage * (1.0 - surface.thinWeight), 1e-8);
    }
#endif
    float energy = max(max(surface.thinReflection.r, surface.thinReflection.g), surface.thinReflection.b) +
                   max(max(surface.diffuseTransmission.r, surface.diffuseTransmission.g), surface.diffuseTransmission.b);
    float keep = energy > 0.0 ? min(1.0, surface.thinWeight / energy) : 1.0;
    surface.thinReflection *= keep;
    surface.diffuseTransmission *= keep;
  }
#endif
#if SURFACE_BASE_LOCATION == 0
  SURFACE_SHADOW_RECEIVER receiverSlope = sunShadowReceiverSlope(lightSpace);
#endif
#if LIT_KIND == 1
  if (materialCoverage(surface, tap.a, colour.a) < surface.cut) { discard; }
#endif
#if SURFACE_BASE_LOCATION > 0
  outputMaterial(albedo, roughness, metalness);
  outputSurface(vec4(0.0), shadingNormal, surface.identity);
#else
  vec4 shaded = vec4(shadeRow(surface, localPosition, shadingNormal, position, albedo,
      metalness, roughness, f0, f90, emission, tangentDir, lightSpace, receiverSlope, shadowMap), 1.0);
#if LIT_KIND == 2
  shaded.a = surface.factor * tap.a * colour.a;
#endif
#if LIT_KIND == 3
  vec3 medium = surfaceMediumTransmittance(surface.attenuationColour, surface.thickness,
                                           surface.attenuationDistance);
  vec3 view = normalize(lights.viewPosition.xyz - position * lights.viewPosition.w);
  vec3 reflected = brdfFresnel(f0, f90, clamp(dot(shadingNormal, view), 0.0, 1.0));
  vec3 transmitted = (vec3(1.0) - reflected) * (1.0 - metalness) * surface.transmission;
  SurfaceTransfer transfer = SurfaceTransfer(shaded.rgb, albedo * medium * transmitted);
  shaded.rgb = surfaceTransferBehind(transfer, texelFetch(behindMap, ivec2(gl_FragCoord.xy), 0).rgb);
#endif
  outputSurface(shaded, shadingNormal, surface.identity);
#endif
}

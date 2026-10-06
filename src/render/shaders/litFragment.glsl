#version 450
#extension GL_GOOGLE_include_directive : require
#define VARYING in
#include "subjectVaryings.glsl"
#include "subjectOutput.glsl"
#include "material.glsl"
#include "subjectLights.glsl"
#include "brdfTables.glsl"
const float kPi = acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "sheenLobe.glsl"
#include "iridescenceLobe.glsl"
#include "microfacetEnergy.glsl"
#if SUBJECT_BASE_LOCATION == 0
#include "subjectLighting.glsl"
#endif
#include "surfaceGradient.glsl"
#include "facadePattern.glsl"
#if LIT_MAPPED
layout(location = 9) in vec4 tangent;
#include "normalFromMap.glsl"
#endif
vec2 coordinate(vec3 u, vec3 v, float second) {
  vec3 homogeneous = vec3(mix(uv, uv1, second), 1.0);
  return vec2(dot(u, homogeneous), dot(v, homogeneous));
}
void main() {
  M surface = materialAt();
  vec4 tap = vec4(1.0);
  vec4 orm = vec4(1.0);
  vec3 emission = surface.emissive;
  vec3 f0 = surface.f0;
  float f90 = surface.specularWeight;
  float roughness = surface.roughness;
  vec3 tangentDir = vec3(0.0);
  vec3 shadingNormal = facing(normal, gl_FrontFacing);
#if LIT_TEXTURED
  tap = texture(colourMap, coordinate(surface.colourUvU, surface.colourUvV, surface.colourUvSecond));
  orm = texture(metalRoughMap, coordinate(surface.metalRoughUvU, surface.metalRoughUvV, surface.metalRoughUvSecond));
#if SUBJECT_BASE_LOCATION == 0
  emission *= texture(emissiveMap, coordinate(surface.emissiveUvU, surface.emissiveUvV, surface.emissiveUvSecond)).rgb;
  float strength = texture(specularStrengthMap, coordinate(surface.specularStrengthUvU, surface.specularStrengthUvV, surface.specularStrengthUvSecond)).a;
  f0 *= strength * texture(specularTintMap, coordinate(surface.specularTintUvU, surface.specularTintUvV, surface.specularTintUvSecond)).rgb;
  f90 *= strength;
#endif
#endif
  roughness *= orm.g;
#if LIT_KIND == 1
  if (surface.factor * tap.a * colour.a < surface.cut) { discard; }
#endif
#if LIT_MAPPED
  vec4 mapped = texture(normalMap, coordinate(surface.normalUvU, surface.normalUvV, surface.normalUvSecond));
  shadingNormal = normalFromMap(normal, tangent, mapped.xyz * 2.0 - 1.0, surface.normalScale, gl_FrontFacing);
  roughness = roughenedBy(roughness, mapped.w);
  tangentDir = tangent.xyz;
#endif
  vec3 albedo = surface.base.rgb * tap.rgb;
  float metalness = surface.metalness * orm.b;
  if (surface.pattern > 0.5) {
    vec2 heightChange;
    applyFacade(uv, colour.rgb, position, localPosition, facing(normal, gl_FrontFacing), albedo, roughness, heightChange);
    shadingNormal = bumpNormal(facing(normal, gl_FrontFacing), shadingNormal, dFdx(position), dFdy(position), heightChange.x, heightChange.y);
  }
  else { albedo *= colour.rgb; }
#if SUBJECT_BASE_LOCATION > 0
  outputMaterial(albedo, roughness, metalness);
  outputSurface(vec4(0.0), shadingNormal, surface.identity);
#else
  vec4 shaded = vec4(shadeRow(surface, localPosition, shadingNormal, position, albedo,
      metalness, roughness, f0, f90, emission, tangentDir, lightSpace, shadowMap), 1.0);
#if LIT_KIND == 2
  shaded.a = surface.factor * tap.a * colour.a;
#endif
#if LIT_KIND == 3
  vec3 medium = vec3(1.0);
  if (surface.thickness > 0.0 && !isinf(surface.attenuationDistance)) {
    medium = exp(log(max(surface.attenuationColour, vec3(1e-5))) *
                 (surface.thickness / surface.attenuationDistance));
  }
  vec3 view = normalize(lights.viewPosition.xyz - position * lights.viewPosition.w);
  vec3 reflected = brdfFresnel(f0, f90, clamp(dot(shadingNormal, view), 0.0, 1.0));
  vec3 transmitted = (vec3(1.0) - reflected) * (1.0 - metalness) * surface.transmission;
  shaded.rgb += texelFetch(behindMap, ivec2(gl_FragCoord.xy), 0).rgb * albedo * medium * transmitted;
#endif
  outputSurface(shaded, shadingNormal, surface.identity);
#endif
}

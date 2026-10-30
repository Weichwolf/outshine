#version 450
#extension GL_GOOGLE_include_directive : require
#include "material.glsl"
#include "lighting.glsl"
#include "brdfTables.glsl"
const float kPi = acos(-1.0);
#include "metalRoughBrdf.glsl"
#include "sheenLobe.glsl"
#include "iridescenceLobe.glsl"
#include "microfacetEnergy.glsl"
layout(set = 2, binding = 0) uniform sampler2D shadowMap;
#define ENVIRONMENT_SPECULAR_BINDING 1
#include "environmentSpecular.glsl"
#include "surfaceLighting.glsl"
#include "packedNormal.glsl"
layout(std430, set = 0, binding = 0) readonly buffer Pixels { uvec4 words[]; };
layout(std140, set = 1, binding = 0) uniform RasterFrame {
  vec4 rightExtent;
  vec4 upExtent;
  vec4 forwardNear;
  vec4 farRaster;
} raster;
layout(location = 0) out vec4 colour;
void main() {
  uvec2 extent = uvec2(raster.farRaster.yz);
  uvec2 at = uvec2(gl_FragCoord.xy);
  uint index = at.y * extent.x + at.x;
  uvec4 pixel = words[2u * index];
  if (pixel.w == 0u) { discard; }
  vec4 properties = uintBitsToFloat(words[2u * extent.x * extent.y + pixel.w - 1u]);
  vec4 baseRough = uintBitsToFloat(words[2u * index + 1u]);
  float depth = uintBitsToFloat(pixel.y);
  float metalness = uintBitsToFloat(pixel.z);
  vec2 uv = (vec2(at) + 0.5) / vec2(extent);
  vec3 position = raster.rightExtent.xyz * ((2.0 * uv.x - 1.0) * raster.rightExtent.w) +
                  raster.upExtent.xyz * ((1.0 - 2.0 * uv.y) * raster.upExtent.w) +
                  raster.forwardNear.xyz * mix(raster.farRaster.x, raster.forwardNear.w, depth);
  M surface = materialAt();
  surface.scattering = properties.w;
  surface.diffuseTransmission = properties.xyz;
  surface.thinWeight = surface.scattering > 0.5 && surface.scattering < 1.5 ? 1.0 : 0.0;
  surface.thinReflection = baseRough.rgb;
  if (surface.scattering > 0.5) { conserveThinSheetEnergy(surface); }
  vec3 normal = unpackedNormal(pixel.x);
  colour = vec4(shadeRow(surface, position, normal, position, baseRough.rgb, metalness,
                          baseRough.a, surface.f0, surface.specularWeight, vec3(0.0),
                          vec3(0.0), vec4(0.0, 0.0, 0.0, 1.0), vec2(0.0), shadowMap), 1.0);
  gl_FragDepth = depth;
}

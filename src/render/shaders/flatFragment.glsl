#version 450
#extension GL_GOOGLE_include_directive : require
#define FLAT_MATERIAL (FLAT_TEXTURED || FLAT_KIND != 0 || SUBJECT_IDENTITY_LOCATION > 0)
#if FLAT_MATERIAL
#include "material.glsl"
#endif
#define VARYING in
#include "flatVaryings.glsl"
#if FLAT_TEXTURED || FLAT_KIND == 3
layout(set = 2, binding = 0) uniform sampler2D imageMap;
#endif
#include "subjectOutput.glsl"
#include "surfaceTransfer.glsl"
void main() {
  float identity = 0.0;
#if FLAT_MATERIAL
  M surface = materialAt();
  identity = surface.identity;
#endif
  vec4 tap = vec4(1.0);
#if FLAT_TEXTURED
  tap = texture(imageMap, materialColourUv(surface, uv, uv1));
#endif
#if FLAT_KIND == 1
  if (materialCoverage(surface, tap.a, colour.a) < surface.cut) { discard; }
#endif
  vec4 shaded = vec4(emitted * tap.rgb * colour.rgb, 1.0);
#if FLAT_KIND == 2
  shaded.a = surface.factor * tap.a * colour.a;
#endif
#if FLAT_KIND == 3
  vec3 medium = surfaceMediumTransmittance(surface.attenuationColour, surface.thickness,
                                           surface.attenuationDistance);
  SurfaceTransfer transfer = SurfaceTransfer(shaded.rgb, surface.base.rgb * colour.rgb * medium *
                                                         surface.transmission);
  shaded.rgb = surfaceTransferBehind(transfer, texelFetch(imageMap, ivec2(gl_FragCoord.xy), 0).rgb);
#endif
  outputMaterial(shaded.rgb, 1.0, 0.0);
  outputSurface(shaded, vec3(0.0), identity);
#if SUBJECT_NORMAL_LOCATION > 0
  outNormal.w = 1.0;
#endif
}

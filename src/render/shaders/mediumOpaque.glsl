#ifndef OUTSHINE_MEDIUM_OPAQUE_GLSL
#define OUTSHINE_MEDIUM_OPAQUE_GLSL

#include "mediumViewAir.glsl"

vec4 mediumClearOpaque(MediumView frame,
                       vec2 fragmentPixel,
                       sampler2D scene,
                       sampler2D depth,
                       sampler2D multiScatter,
                       sampler2D veil) {
  ivec2 pixel = ivec2(fragmentPixel);
  vec4 lit = texelFetch(scene, pixel, 0);
  float written = texelFetch(depth, pixel, 0).r;
  if (written <= 0.0) { return lit; }
  MediumViewRay view = mediumViewRay(frame, vec3(fragmentPixel, written));
  AirTransfer air = mediumViewAir(frame, view, veil, multiScatter);
  return vec4(lit.rgb * air.Transmittance + frame.Illuminance * air.RadiancePerLux, lit.a);
}

#endif

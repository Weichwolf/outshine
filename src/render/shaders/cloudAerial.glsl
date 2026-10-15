#ifndef OUTSHINE_CLOUD_AERIAL_GLSL
#define OUTSHINE_CLOUD_AERIAL_GLSL

#include "cloudRadiance.glsl"
#include "aerialTransmittance.glsl"

struct CloudAirView {
  Medium air;
  float radiusKm;
  float cosZenith;
  vec3 skyRadiance;
};

vec4 cloudAerial(CloudLayer layer, vec3 originM, vec3 direction, float maximumM,
                  CloudLight light, CloudAirView air, sampler2D veil) {
  if (layer.field.x <= 0.0) { return vec4(0.0, 0.0, 0.0, 1.0); }
  mat2 intervals = cloudShell(layer, originM, direction, maximumM);
  vec3 radiance = vec3(0.0);
  float through = 1.0;
  for (int interval = 0; interval < 2; ++interval) {
    vec2 span = intervals[interval];
    if (span.y <= span.x) { continue; }
    for (int sampleIndex = 0; sampleIndex < CLOUD_VIEW_SAMPLES; ++sampleIndex) {
      float begin = float(sampleIndex) / float(CLOUD_VIEW_SAMPLES);
      float end = float(sampleIndex + 1) / float(CLOUD_VIEW_SAMPLES);
#if CLOUD_VIEW_WARP == 2
      begin *= begin;
      end *= end;
#endif
      float stepM = (span.y - span.x) * (end - begin);
      float distanceM = span.x + (span.y - span.x) * (begin + end) * 0.5;
      vec3 at = originM + direction * distanceM;
      float extinction = cloudExtinction(layer, at);
      if (extinction <= 0.0) { continue; }
      float stepThrough = exp(-extinction * stepM);
      vec3 clearThrough = aerialTransmittance(air.air, veil, air.radiusKm, air.cosZenith, distanceM * 0.001);
      vec3 clearFront = air.skyRadiance * (1.0 - clearThrough);
      vec3 source = cloudSource(layer, at, direction, light);
      radiance += through * (1.0 - stepThrough) * (clearFront + clearThrough * source);
      through *= stepThrough;
    }
  }
  return vec4(radiance, through);
}

#endif

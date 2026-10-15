#ifndef OUTSHINE_CLOUD_RADIANCE_GLSL
#define OUTSHINE_CLOUD_RADIANCE_GLSL

#include "cloudLayer.glsl"

#ifndef CLOUD_VIEW_SAMPLES
#define CLOUD_VIEW_SAMPLES 16
#endif
#ifndef CLOUD_LIGHT_SAMPLES
#define CLOUD_LIGHT_SAMPLES 6
#endif
#ifndef CLOUD_VIEW_WARP
#define CLOUD_VIEW_WARP 1
#endif

struct CloudLight {
  vec3 toSun;
  vec3 sunIrradiance;
  vec3 skyRadiance;
};

float cloudPhase(float cosine, float g) {
  float square = 1.0 + g * g - 2.0 * g * cosine;
  return (1.0 - g * g) / (12.566370614359 * square * sqrt(square));
}

vec3 cloudSource(CloudLayer layer, vec3 positionM, vec3 direction, CloudLight light) {
  float sunDepth = cloudOpticalDepthSteps(layer, positionM, light.toSun, 1.0e8, CLOUD_LIGHT_SAMPLES);
  float direct = cloudPhase(dot(direction, light.toSun), 0.65) * exp(-sunDepth);
  float multiple = 0.15 * exp(-0.25 * sunDepth) / 12.566370614359;
  return light.sunIrradiance * (direct + multiple) + light.skyRadiance;
}

vec4 cloudRadiance(CloudLayer layer, vec3 originM, vec3 direction, float maximumM,
                    CloudLight light) {
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
      radiance += through * (1.0 - stepThrough) * cloudSource(layer, at, direction, light);
      through *= stepThrough;
    }
  }
  return vec4(radiance, through);
}

#endif

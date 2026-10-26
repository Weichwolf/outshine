#ifndef OUTSHINE_EXPERIMENT_CLOUD_SAMPLING_GAUSS3_GLSL
#define OUTSHINE_EXPERIMENT_CLOUD_SAMPLING_GAUSS3_GLSL

#if CLOUD_LIGHT_SAMPLES != 3
#error The Gauss3 counterexample requires three light samples
#endif

#define cloudOpticalDepthSteps cloudMidpointOpticalDepthSteps
#include "cloudLayer.glsl"
#undef cloudOpticalDepthSteps

float cloudOpticalDepthSteps(CloudLayer layer, vec3 positionM, vec3 toSun,
                             float maximumM, int sampleCount) {
  if (sampleCount != 3) {
    return cloudMidpointOpticalDepthSteps(layer, positionM, toSun, maximumM, sampleCount);
  }
  mat2 intervals = cloudShell(layer, positionM, toSun, maximumM);
  float optical = 0.0;
  for (int interval = 0; interval < 2; ++interval) {
    vec2 span = intervals[interval];
    if (span.y <= span.x) { continue; }
    float widthM = span.y - span.x;
    for (int sampleIndex = 0; sampleIndex < 3; ++sampleIndex) {
      float fraction = 0.5 + 0.3872983346207417 * float(sampleIndex - 1);
      float weight = sampleIndex == 1 ? 0.4444444444444444 : 0.2777777777777778;
      float distanceM = span.x + fraction * widthM;
      optical += cloudExtinction(layer, positionM + toSun * distanceM) * (weight * widthM);
    }
  }
  return optical;
}

#endif

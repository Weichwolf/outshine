#ifndef OUTSHINE_CLOUD_PREFIX_GLSL
#define OUTSHINE_CLOUD_PREFIX_GLSL

#include "cloudRadiance.glsl"

#if CLOUD_VIEW_SAMPLES < 1
#error CLOUD_VIEW_SAMPLES must be positive
#endif
#if CLOUD_VIEW_WARP != 1 && CLOUD_VIEW_WARP != 2
#error CLOUD_VIEW_WARP must be one or two
#endif

struct CloudPrefix {
  CloudLayer Layer;
  CloudLight Light;
  vec3 OriginM;
  vec3 Direction;
  mat2 Intervals;
  uint NextCell;
  float ExtentM;
  float BeginM;
  float EndM;
  float ExtinctionPerKm;
  vec3 EmissionPerKmPerLux;
  bool Evaluated;
  uint DensityEvaluations;
  uint SourceEvaluations;
};

void cloudPrefixNext(inout CloudPrefix prefix) {
  prefix.BeginM = prefix.ExtentM;
  prefix.EndM = prefix.ExtentM;
  prefix.ExtinctionPerKm = 0.0;
  prefix.EmissionPerKmPerLux = vec3(0.0);
  prefix.Evaluated = false;
  while (prefix.NextCell < 2u * uint(CLOUD_VIEW_SAMPLES)) {
    uint index = prefix.NextCell++;
    vec2 interval = prefix.Intervals[index / uint(CLOUD_VIEW_SAMPLES)];
    if (interval.y <= interval.x) { continue; }
    uint cell = index % uint(CLOUD_VIEW_SAMPLES);
    float begin = float(cell) / float(CLOUD_VIEW_SAMPLES);
    float end = float(cell + 1u) / float(CLOUD_VIEW_SAMPLES);
#if CLOUD_VIEW_WARP == 2
    begin *= begin;
    end *= end;
#endif
    prefix.BeginM = interval.x + (interval.y - interval.x) * begin;
    prefix.EndM = interval.x + (interval.y - interval.x) * end;
    if (prefix.EndM > prefix.BeginM) { return; }
  }
  prefix.BeginM = prefix.ExtentM;
  prefix.EndM = prefix.ExtentM;
}

CloudPrefix cloudPrefixBegin(CloudLayer layer, vec3 originM, vec3 direction,
                             float extentM, CloudLight light) {
  CloudPrefix prefix;
  prefix.Layer = layer;
  prefix.Light = light;
  prefix.OriginM = originM;
  prefix.Direction = direction;
  prefix.ExtentM = extentM;
  prefix.Intervals = layer.field.x > 0.0 ? cloudShell(layer, originM, direction, extentM) : mat2(0.0);
  prefix.NextCell = layer.field.x > 0.0 ? 0u : 2u * uint(CLOUD_VIEW_SAMPLES);
  prefix.DensityEvaluations = 0u;
  prefix.SourceEvaluations = 0u;
  cloudPrefixNext(prefix);
  return prefix;
}

void cloudPrefixEvaluate(inout CloudPrefix prefix) {
  if (prefix.Evaluated || prefix.EndM <= prefix.BeginM) { return; }
  vec3 at = prefix.OriginM + prefix.Direction * (0.5 * (prefix.BeginM + prefix.EndM));
  float extinction = cloudExtinction(prefix.Layer, at);
  ++prefix.DensityEvaluations;
  prefix.ExtinctionPerKm = extinction * 1000.0;
  if (extinction > 0.0) {
    prefix.EmissionPerKmPerLux = prefix.ExtinctionPerKm *
        cloudSource(prefix.Layer, at, prefix.Direction, prefix.Light);
    ++prefix.SourceEvaluations;
  }
  prefix.Evaluated = true;
}

#endif

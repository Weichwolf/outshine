#ifndef OUTSHINE_MEDIUM_VIEW_AIR_GLSL
#define OUTSHINE_MEDIUM_VIEW_AIR_GLSL

#include "mediumView.glsl"
#include "airPrefix.glsl"

AirTransfer
mediumViewAir(MediumView frame, MediumViewRay view, sampler2D veil, sampler2D multiScatter) {
  AirPathSpan span = airPhysicalSpan(frame.Air, view.Air);
  if (span.EndKm <= span.BeginKm || view.Air.DistanceKm <= span.BeginKm) {
    return AirTransfer(vec3(0.0), vec3(1.0));
  }
  AirRay ray = airPathRay(frame.Air, view.Air, span);
  AirPrefix prefix = airPrefixBegin(frame.Air, ray, veil, multiScatter);
  float queryKm = min(view.Air.DistanceKm - span.BeginKm, ray.DistanceKm);
  return airPrefixAdvance(prefix, frame.Air, queryKm, veil, multiScatter);
}

#endif

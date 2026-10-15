#ifndef OUTSHINE_CLOUD_AIR_SOURCE_GLSL
#define OUTSHINE_CLOUD_AIR_SOURCE_GLSL

#include "airSource.glsl"
#include "cloudRadiance.glsl"

AirSource cloudShadedAirSource(Medium medium, AirRay ray, float distanceKm, bool sunVisible,
                               sampler2D veil, sampler2D multiScatter, AirSourceRay sourceRay,
                               CloudLayer layer, vec3 toSun) {
  if (!sunVisible || layer.field.x <= 0.0 || layer.shell.w <= 0.0) {
    return airSourceAt(medium, ray, distanceKm, sunVisible, veil, multiScatter);
  }
  AirSourceTerms terms = airSourceTermsAt(medium, ray, distanceKm, sunVisible, veil, multiScatter);
  vec3 pointM = sourceRay.OriginM + sourceRay.Direction * (distanceKm * 1000.0);
  float cloudSunT = exp(-cloudOpticalDepthSteps(layer, pointM, toSun, 1.0e8, CLOUD_LIGHT_SAMPLES));
  return AirSource(terms.Extinction, terms.DirectEmission * cloudSunT + terms.DiffuseEmission);
}

#endif

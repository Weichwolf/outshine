#ifndef OUTSHINE_EXPERIMENT_AIR_DEPTH_SOURCE_GLSL
#define OUTSHINE_EXPERIMENT_AIR_DEPTH_SOURCE_GLSL

#include "cloud_air_transfer_response.glsl"
#ifndef SOURCE_DEPTH_INTERVALS
#define SOURCE_DEPTH_INTERVALS 512
#endif
#ifndef SOURCE_DEPTH_GROWTH
#define SOURCE_DEPTH_GROWTH 0.0
#endif
#if SOURCE_DEPTH_INTERVALS < 1 || SOURCE_DEPTH_INTERVALS > 1024
#error Source depth intervals must be in 1..1024
#endif

struct DepthSourcePrefix {
  vec3 MassAtStart;
  float BeginKm;
  float EndKm;
  float SolarA;
  float SolarB;
  uint Grid;
  uint Evaluations;
};

float depthSourceSolar(AirSourceRay ray, float distanceKm) {
  vec3 pointM = ray.OriginM + ray.Direction * (distanceKm * 1000.0);
  return exp(-cloudOpticalDepthSteps(
      sourceWeatherLayer(), pointM, sourceSunDirection(), 1.0e8, CLOUD_LIGHT_SAMPLES));
}

float depthSourceBoundary(float extentKm, uint index) {
  if (index >= uint(SOURCE_DEPTH_INTERVALS)) { return extentKm; }
  if (SOURCE_DEPTH_GROWTH > 0.0) {
    return min(extentKm, exp2(SOURCE_DEPTH_GROWTH * 1.44269504089 * float(index)) - 1.0);
  }
  float fraction = float(index) / float(SOURCE_DEPTH_INTERVALS);
  return 0.1 * (exp2(log2(1.0 + extentKm / 0.1) * fraction) - 1.0);
}

vec3 depthSourceFirstMoment(vec3 sigma, float distanceKm, vec3 integral) {
  vec3 tau = sigma * distanceKm;
  vec3 result;
  for (int c = 0; c < 3; ++c) {
    float t = tau[c];
    if (t < 0.25) {
      result[c] =
          distanceKm * distanceKm *
          (0.5 +
           t * (-1.0 / 3.0 +
                t * (1.0 / 8.0 +
                     t * (-1.0 / 30.0 +
                          t * (1.0 / 144.0 +
                               t * (-1.0 / 840.0 +
                                    t * (1.0 / 5760.0 + t * (-1.0 / 45360.0 + t / 403200.0))))))));
    } else {
      result[c] = (integral[c] - distanceKm * exp(-t)) / sigma[c];
    }
  }
  return result;
}

vec3 depthSourcePartial(DepthSourcePrefix prefix, vec3 sigma, float distanceKm) {
  vec3 integral = transferIntegral(sigma, distanceKm);
  vec3 weightB =
      depthSourceFirstMoment(sigma, distanceKm, integral) / (prefix.EndKm - prefix.BeginKm);
  return exp(-sigma * prefix.BeginKm) *
         ((integral - weightB) * prefix.SolarA + weightB * prefix.SolarB);
}

DepthSourcePrefix depthSourceBegin(AirRay ray, AirSourceRay metric) {
  DepthSourcePrefix prefix;
  prefix.MassAtStart = vec3(0.0);
  prefix.BeginKm = 0.0;
  prefix.EndKm = depthSourceBoundary(ray.DistanceKm, 1u);
  prefix.SolarA = depthSourceSolar(metric, 0.0);
  prefix.SolarB = depthSourceSolar(metric, prefix.EndKm);
  prefix.Grid = 1u;
  prefix.Evaluations = 2u;
  return prefix;
}

vec3 depthSourceAdvance(
    inout DepthSourcePrefix prefix, AirRay ray, AirSourceRay metric, vec3 sigma, float queryKm) {
  for (int step = 0; step < SOURCE_DEPTH_INTERVALS; ++step) {
    if (queryKm < prefix.EndKm || prefix.EndKm >= ray.DistanceKm) { break; }
    prefix.MassAtStart += depthSourcePartial(prefix, sigma, prefix.EndKm - prefix.BeginKm);
    prefix.BeginKm = prefix.EndKm;
    prefix.SolarA = prefix.SolarB;
    prefix.EndKm = depthSourceBoundary(ray.DistanceKm, ++prefix.Grid);
    prefix.SolarB = depthSourceSolar(metric, prefix.EndKm);
    ++prefix.Evaluations;
  }
  return prefix.MassAtStart + depthSourcePartial(prefix, sigma, queryKm - prefix.BeginKm);
}

void main() {
  uint index = gl_GlobalInvocationID.x;
  if (index >= uint(from.rays.length())) { return; }
  ProbeRay inputRay = from.rays[index];
  float azimuth = uintBitsToFloat(inputRay.Pad);
  float horizontal = sqrt(max(0.0, 1.0 - inputRay.Ray.CosView * inputRay.Ray.CosView));
  vec3 direction = vec3(horizontal * cos(azimuth), inputRay.Ray.CosView, horizontal * sin(azimuth));
  vec3 originM = vec3(0.0, (inputRay.Ray.RadiusKm - pushed.Air.BottomRadiusKm) * 1000.0, 0.0);
  AirPathSpan span = airPhysicalSpan(pushed.Air, inputRay.Ray);
  AirRay ray = airPathRay(pushed.Air, inputRay.Ray, span);
  AirSourceRay metric = AirSourceRay(originM + direction * (span.BeginKm * 1000.0), direction);
  AirSourceTerms terms = airSourceTermsAt(
      pushed.Air, ray, 0.0, airSunVisible(pushed.Air, ray, 0.0), veil, multiScatter);
  bool clear = sourceWeatherLayer().field.x <= 0.0;
  if (ray.DistanceKm <= 0.0 || clear) {
    for (uint at = inputRay.QueryBegin; at < inputRay.QueryEnd; ++at) {
      float distanceKm = clamp(query.distanceKm[at] - span.BeginKm, 0.0, ray.DistanceKm);
      vec3 radiance = transferIntegral(terms.Extinction, distanceKm) *
                      (terms.DirectEmission + terms.DiffuseEmission);
      vec3 through = exp(-terms.Extinction * distanceKm);
      into.samples[at] =
          ProbeSample(radiance.r, radiance.g, radiance.b, through.r, through.g, through.b, 0u, 0u);
    }
    return;
  }
  DepthSourcePrefix prefix = depthSourceBegin(ray, metric);
  for (uint at = inputRay.QueryBegin; at < inputRay.QueryEnd; ++at) {
    float lengthKm = clamp(query.distanceKm[at] - span.BeginKm, 0.0, ray.DistanceKm);
    vec3 mass = depthSourceAdvance(prefix, ray, metric, terms.Extinction, lengthKm);
    vec3 integral = transferIntegral(terms.Extinction, lengthKm);
    vec3 radiance = terms.DirectEmission * mass + terms.DiffuseEmission * integral;
    vec3 through = exp(-terms.Extinction * lengthKm);
    into.samples[at] = ProbeSample(radiance.r,
                                   radiance.g,
                                   radiance.b,
                                   through.r,
                                   through.g,
                                   through.b,
                                   prefix.Evaluations,
                                   prefix.Grid - 1u);
  }
}

#endif

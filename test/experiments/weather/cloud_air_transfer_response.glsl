#ifndef OUTSHINE_EXPERIMENT_AIR_TRANSFER_RESPONSE_GLSL
#define OUTSHINE_EXPERIMENT_AIR_TRANSFER_RESPONSE_GLSL

vec3 transferIntegral(vec3 sigma, float lengthKm) {
  vec3 tau = sigma * lengthKm;
  vec3 small = lengthKm * (1.0 - tau * (0.5 - tau / 6.0));
  vec3 large = (vec3(1.0) - exp(-tau)) / sigma;
  return mix(large, small, lessThan(tau, vec3(0.01)));
}

#endif

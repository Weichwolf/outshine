#ifndef OUTSHINE_AIR_SOURCE_GLSL
#define OUTSHINE_AIR_SOURCE_GLSL
#include "airRay.glsl"
#include "clearSun.glsl"

struct AirSource {
  vec3 Extinction;
  vec3 Emission;
};

struct AirSourceTerms {
  vec3 Extinction;
  vec3 DirectEmission;
  vec3 DiffuseEmission;
};

struct AirSourceRay {
  vec3 OriginM;
  vec3 Direction;
};

AirSourceTerms airSourceTermsAt(Medium medium, AirRay ray, float distanceKm, bool sunVisible,
                      sampler2D veil, sampler2D multiScatter) {
  float heightKm = max(0.0,
      mediumHeightAlong(medium, ray.RadiusKm, ray.CosView, distanceKm));
  float radiusKm = medium.BottomRadiusKm + heightKm;
  float cosSunAt = clamp(
      (ray.RadiusKm * ray.CosSun + distanceKm * ray.ViewSun) / radiusKm, -1.0, 1.0);
  MediumLook point = MediumLook(radiusKm, cosSunAt);
  vec3 toSun = sunVisible ? clearSunUnoccludedTransfer(medium, point, veil) : vec3(0.0);
  ivec2 extent = textureSize(multiScatter, 0);
  MediumUv uv = multiScatterSample(medium, point,
      MediumLutSize(float(extent.x), float(extent.y)), kMediumGroundLiftKm);
  vec3 psi = textureLod(multiScatter, vec2(uv.U, uv.V), 0.0).rgb;
  vec3 rayleigh = exp(-heightKm / medium.RayleighScaleHeightKm) *
      vec3(medium.RayleighScatteringPerKm);
  vec3 mie = vec3(exp(-heightKm / medium.MieScaleHeightKm) * medium.MieScatteringPerKm);
  AirSourceTerms result;
  result.Extinction = mediumExtinctionPerKm(medium, heightKm);
  result.DirectEmission = toSun * (rayleigh * rayleighPhase(ray.ViewSun) +
      mie * miePhase(medium.MiePhaseG, ray.ViewSun));
  result.DiffuseEmission = (rayleigh + mie) * psi;
  return result;
}

AirSource airSourceAt(Medium medium, AirRay ray, float distanceKm, bool sunVisible,
                      sampler2D veil, sampler2D multiScatter) {
  AirSourceTerms source = airSourceTermsAt(medium, ray, distanceKm, sunVisible, veil, multiScatter);
  return AirSource(source.Extinction, source.DirectEmission + source.DiffuseEmission);
}

AirSource airSourceForRay(Medium medium, AirRay ray, float distanceKm, bool sunVisible,
                          sampler2D veil, sampler2D multiScatter, AirSourceRay sourceRay) {
  return airSourceAt(medium, ray, distanceKm, sunVisible, veil, multiScatter);
}
#endif

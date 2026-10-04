#include "../../content/shade/FacadeOpeningValues.h"

float facadeOpeningCoverage(float bay, float storey, float bayWidth, float storeyWidth) {
  float column = periodicBand(fract(bay), kOpeningLowU, kOpeningHighU, bayWidth);
  float row = periodicBand(fract(storey), kOpeningLowV, kOpeningHighV, storeyWidth);
  return column * row;
}

struct FacadeGlazing {
  vec3 albedo;
  float roughness;
};

FacadeGlazing facadeGlazing(float coverage, vec3 albedo, float roughness) {
  return FacadeGlazing(mix(albedo, vec3(0.025f, 0.035f, 0.045f), coverage),
                       mix(roughness, 0.12f, coverage));
}

#include "FacadeOpeningValues.h"

float facadeOpeningCoverage(float bay, float storey, float bayWidth, float storeyWidth) {
  float column = periodicBand(fract(bay), kOpeningLowU, kOpeningHighU, bayWidth);
  float row = periodicBand(fract(storey), kOpeningLowV, kOpeningHighV, storeyWidth);
  return column * row;
}

struct FacadeGlazing {
  vec3 albedo;
  float roughness;
};

float facadePaneCoverage(float bay, float storey, float bayWidth, float storeyWidth) {
  float left = periodicBand(fract(bay), kPaneLowU, kMullionLowU, bayWidth);
  float right = periodicBand(fract(bay), kMullionHighU, kPaneHighU, bayWidth);
  float row = periodicBand(fract(storey), kPaneLowV, kPaneHighV, storeyWidth);
  return (left + right) * row;
}

FacadeGlazing facadeWindow(float opening, float pane, vec3 albedo, float roughness) {
  float frame = max(0.0f, opening - pane);
  return FacadeGlazing(albedo * (1.0f - opening) +
                           vec3(0.48f, 0.46f, 0.41f) * frame +
                           vec3(0.025f, 0.035f, 0.045f) * pane,
                       roughness * (1.0f - opening) + 0.55f * frame + 0.12f * pane);
}

FacadeGlazing facadeGlazing(float coverage, vec3 albedo, float roughness) {
  return FacadeGlazing(mix(albedo, vec3(0.025f, 0.035f, 0.045f), coverage),
                       mix(roughness, 0.12f, coverage));
}

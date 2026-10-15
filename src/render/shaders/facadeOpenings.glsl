#include "FacadeOpeningValues.h"

float facadeOpeningCoverage(vec2 fractions, float bay, float storey, float bayWidth, float storeyWidth) {
  float column = periodicBand(fract(bay), (1.0f - fractions.x) * 0.5f,
                              (1.0f + fractions.x) * 0.5f, bayWidth);
  float row = periodicBand(fract(storey), (1.0f - fractions.y) * 0.5f,
                           (1.0f + fractions.y) * 0.5f, storeyWidth);
  return column * row;
}

struct FacadeGlazing {
  vec3 albedo;
  float roughness;
};

float facadePaneCoverage(vec2 fractions, float bay, float storey, float bayWidth, float storeyWidth) {
  if (fractions.x <= 2.0f * kPaneInset || fractions.y <= 2.0f * kPaneInset) { return 0.0f; }
  float left = periodicBand(fract(bay), (1.0f - fractions.x) * 0.5f + kPaneInset,
                            kMullionLowU, bayWidth);
  float right = periodicBand(fract(bay), kMullionHighU,
                             (1.0f + fractions.x) * 0.5f - kPaneInset, bayWidth);
  float row = periodicBand(fract(storey), (1.0f - fractions.y) * 0.5f + kPaneInset,
                           (1.0f + fractions.y) * 0.5f - kPaneInset, storeyWidth);
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

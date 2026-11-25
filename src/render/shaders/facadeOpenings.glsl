#include "FacadeOpeningValues.h"

struct FacadeFinish {
  vec3 colour;
  float roughness;
};

#define FACADE_FINISH(name, red, green, blue, roughness) \
  const FacadeFinish name = FacadeFinish(vec3(red, green, blue), roughness);
#include "FacadeFinishes.inc"
#undef FACADE_FINISH

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
                           kFacadeFrameFinish.colour * frame + kFacadePaneFinish.colour * pane,
                       roughness * (1.0f - opening) +
                           kFacadeFrameFinish.roughness * frame + kFacadePaneFinish.roughness * pane);
}

FacadeGlazing facadeGlazing(float coverage, vec3 albedo, float roughness) {
  return FacadeGlazing(mix(albedo, kFacadePaneFinish.colour, coverage),
                       mix(roughness, kFacadePaneFinish.roughness, coverage));
}

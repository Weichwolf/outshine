#include "BuildingMaterials.h"
#include "FacadeOpeningValues.h"
#include "FacadeProfiles.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace outshine::Generators {
namespace {
constexpr float kWallRoughness = 0.88f;
constexpr float kTileRed = 0.42f;
constexpr float kTileGreen = 0.20f;
constexpr float kTileBlue = 0.14f;
constexpr float kTileRoughness = 0.72f;

#define FACADE_FINISH(Name, Red, Green, Blue, Roughness)                                           \
  constexpr Vec4f Name{{Red, Green, Blue, Roughness}};
#include "FacadeFinishes.inc"
#undef FACADE_FINISH

#define ROOF_FINISH(Name, Red, Green, Blue, Roughness)                                             \
  constexpr Vec4f Name{{Red##f, Green##f, Blue##f, Roughness##f}};
#define ROOF_VARIANTS(A, B, C, D, E, F, G, H)                                                      \
  constexpr std::array kPitchedRoofFinishes{A, B, C, D, E, F, G, H};
#include "RoofFinishes.inc"
#undef ROOF_VARIANTS
#undef ROOF_FINISH
}

std::array<Material, 3> BuildingSurfaceMaterials() {
  Material walls;
  for (size_t channel = 0; channel < 3; ++channel) {
    walls.BaseColour[channel] = kBuildingWallColour[channel];
  }
  walls.Roughness = kWallRoughness;
  walls.Pattern = SurfacePattern::Facade;
  Material roof;
  roof.BaseColour[0] = kTileRed;
  roof.BaseColour[1] = kTileGreen;
  roof.BaseColour[2] = kTileBlue;
  roof.Roughness = kTileRoughness;
  roof.Pattern = SurfacePattern::Facade;
  Material metreWalls = walls;
  metreWalls.Pattern = SurfacePattern::FacadeMetres;
  return {walls, roof, metreWalls};
}

Material AverageHouseWallMaterial() {
  const auto &profile = FacadeProfileOf(FacadeStyle::House);
  const float opening = profile.OpeningWidth * profile.OpeningHeight;
  const float wall = 1.0f - opening;
  const float paneWidth = profile.OpeningWidth - 2 * kPaneInset - (kMullionHighU - kMullionLowU);
  const float paneHeight = profile.OpeningHeight - 2 * kPaneInset;
  const float pane = std::max(0.0f, paneWidth) * std::max(0.0f, paneHeight);
  const float frame = opening - pane;
  const float band = std::min(kStoreyBandHeightM / static_cast<float>(profile.FloorM),
                              1.0f - profile.OpeningHeight);
  Material material;
  for (size_t channel = 0; channel < 3; ++channel) {
    material.BaseColour[channel] =
        kBuildingWallColour[channel] * (wall - kStoreyBandDarkening * band) +
        frame * kFacadeFrameFinish[channel] + pane * kFacadePaneFinish[channel];
  }
  material.Roughness = std::sqrt(wall * kWallRoughness * kWallRoughness +
                                 frame * kFacadeFrameFinish[3] * kFacadeFrameFinish[3] +
                                 pane * kFacadePaneFinish[3] * kFacadePaneFinish[3]);
  return material;
}

Vec4f BuildingRoofCoating(float pitchedFraction, const Vec3f &wallColour) {
  Vec3f mean;
  float alphaSum = 0.0f;
  for (const auto &finish : kPitchedRoofFinishes) {
    for (size_t channel = 0; channel < 3; ++channel) { mean[channel] += finish[channel]; }
    const float roughness = std::max(kTileRoughness, finish[3]);
    alphaSum += roughness * roughness;
  }
  constexpr std::array base{kTileRed, kTileGreen, kTileBlue};
  constexpr auto count = static_cast<float>(kPitchedRoofFinishes.size());
  Vec4f result;
  for (size_t channel = 0; channel < 3; ++channel) {
    const float tint = std::lerp(kFlatRoofFinish[channel], mean[channel] / count, pitchedFraction);
    result[channel] = tint * base[channel] / wallColour[channel];
  }
  const float flatRoughness = kFlatRoofFinish[3];
  result[3] =
      std::sqrt(std::lerp(flatRoughness * flatRoughness, alphaSum / count, pitchedFraction));
  return result;
}
}

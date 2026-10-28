#include "BuildingMaterials.h"
#include <array>
#include <cstddef>

namespace outshine::Generators {
namespace {
constexpr float kWallRoughness = 0.88f;
constexpr float kTileRed = 0.42f;
constexpr float kTileGreen = 0.20f;
constexpr float kTileBlue = 0.14f;
constexpr float kTileRoughness = 0.72f;
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
}

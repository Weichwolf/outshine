#ifndef OUTSHINE_GENERATORS_TERRAIN_INPUTS_GROUNDMESHER_H
#define OUTSHINE_GENERATORS_TERRAIN_INPUTS_GROUNDMESHER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <Earth.h>

#include "Vec3.h"
#include "Box.h"
#include "ViewPath.h"

#include "TileMeshes.h"
#include "EarthworkStamp.h"
#include "TerrainPatchwork.h"

namespace outshine {

constexpr int kPatchGrid = 32;

constexpr double kBatterRun = 1.5;
constexpr double kBatterRise = 1.0 / kBatterRun;
constexpr double kMostEarthworkM = 30.0;
constexpr double kLeastApronM = 3.0;
constexpr double kMostApronM = kMostEarthworkM * kBatterRun;
constexpr double kStampWorthM = 0.25;
constexpr double kBrokenGroundM = 1.0;

enum class TerrainTileAccess : uint8_t { Mesh, Request, Inspect };

struct TerrainSweep {
  std::vector<Data::TileId> Requested;
  std::vector<Data::TileId> Patches;
  LongitudeLatitude Focus{};
  Box Eyes{};
  std::optional<ViewPath> EyePath = std::nullopt;
  int Levels = 1;
  uint64_t Region = 0;
};

struct Around {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  int Zoom = 0;
  int Levels = 1;
  int Grid = kPatchGrid;
  TerrainTileAccess Access = TerrainTileAccess::Mesh;
  bool PlayableOnly = false;
  std::shared_ptr<const TerrainSweep> Sweep = nullptr;
};

class GroundMesher {
public:
  virtual ~GroundMesher() = default;
  GroundMesher(const GroundMesher &) = delete;
  GroundMesher &operator=(const GroundMesher &) = delete;

  [[nodiscard]] virtual std::expected<Patchwork, std::string> Lay(TileMeshes &tiles,
                                                                  const Around &over) const = 0;

protected:
  GroundMesher() = default;
};
}
#endif

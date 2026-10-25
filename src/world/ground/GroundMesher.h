#ifndef OUTSHINE_WORLD_GROUND_GROUNDMESHER_H
#define OUTSHINE_WORLD_GROUND_GROUNDMESHER_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include <Earth.h>
#include "EarthworkStamp.h"

#include "Vec3.h"

#include "TileMeshes.h"

namespace outshine {

constexpr size_t kZoomLevels = 24;

constexpr int kPatchGrid = 32;

constexpr double kBatterRun = 1.5;
constexpr double kBatterRise = 1.0 / kBatterRun;
constexpr double kMostEarthworkM = 30.0;
constexpr double kLeastApronM = 3.0;
constexpr double kMostApronM = kMostEarthworkM * kBatterRun;
constexpr double kStampWorthM = 0.25;
constexpr double kBrokenGroundM = 1.0;

enum class TerrainTileAccess : uint8_t { Mesh, Request, Inspect };

struct Around {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  int Zoom = 0;
  int Levels = 1;
  int Grid = kPatchGrid;
  TerrainTileAccess Access = TerrainTileAccess::Mesh;
  bool PlayableOnly = false;
};

struct Sheet {
  Data::TileId Tile;
  std::vector<float> Nodes;
  int Side = 0;
  uint32_t Postings = 0;
  bool Virtual = false;
  int SourceZoom = -1;

  [[nodiscard]] bool operator==(const Sheet &) const = default;
};

struct Patchwork {
  std::vector<Sheet> Sheets;
  size_t Tiles = 0;
  size_t Pending = 0;
  size_t Absent = 0;
  size_t Refused = 0;
  size_t Skipped = 0;
  size_t Bare = 0;
  size_t ContactPending = 0;
  std::array<int, kZoomLevels> PendingAtZoom = {{}};
  std::array<int, kZoomLevels> WantedAtZoom = {{}};
  size_t Overlapped = 0;
  long ReachTiles = 0;
  int CoarsestZoom = 0;

  [[nodiscard]] size_t HeapBytes() const noexcept {
    size_t bytes = Sheets.capacity() * sizeof(Sheet);
    for (const Sheet &sheet : Sheets) { bytes += sheet.Nodes.capacity() * sizeof(float); }
    return bytes;
  }
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

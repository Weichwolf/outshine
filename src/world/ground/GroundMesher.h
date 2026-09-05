#ifndef OUTSHINE_WORLD_GROUND_GROUNDMESHER_H
#define OUTSHINE_WORLD_GROUND_GROUNDMESHER_H

#include <array>
#include <cstdint>
#include <expected>
#include <span>
#include <string>
#include <vector>

#include "math/Vec3.h"

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

struct Around {
  double LatitudeDeg = 0.0;
  double LongitudeDeg = 0.0;
  int Zoom = 0;
  int Levels = 1;
  int Grid = kPatchGrid;
  bool Asking = false;
};

struct Sheet {
  Data::TileId Tile;
  std::vector<float> Nodes;
  int Side = 0;
  uint32_t Postings = 0;
  bool Virtual = false;
};

struct Patchwork {
  std::vector<Sheet> Sheets;
  size_t Tiles = 0;
  size_t Pending = 0;
  size_t Absent = 0;
  size_t Refused = 0;
  size_t Skipped = 0;
  size_t Bare = 0;
  std::array<int, kZoomLevels> PendingAtZoom = {{}};
  std::array<int, kZoomLevels> WantedAtZoom = {{}};
  size_t Overlapped = 0;
  long ReachTiles = 0;
  int CoarsestZoom = 0;
};

struct EastSouth {
  double EastM = 0.0;
  double SouthM = 0.0;
};

enum class Stamp : uint8_t { Pad, Corridor, Basin };

struct Yields {
  std::vector<double> RingEastSouthM;
  double LowE = 0.0, HighE = 0.0, LowS = 0.0, HighS = 0.0;
  double AtE = 0.0, AtS = 0.0;
  double PlateauM = 0.0;
  double SlopeE = 0.0, SlopeS = 0.0;
  std::vector<double> SeamEastSouthM;
  double ApronM = 0.0;
  double YieldM = 0.0;
  double SagInv = 0.0;
  bool Fills = false;
  Stamp Kind = Stamp::Pad;

  [[nodiscard]] double WantsAt(EastSouth at) const {
    const double dE = at.EastM - AtE;
    const double dS = at.SouthM - AtS;
    return PlateauM + SlopeE * dE + SlopeS * dS - 0.5 * (dE * dE + dS * dS) * SagInv;
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
} // namespace outshine
#endif

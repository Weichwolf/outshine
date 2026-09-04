#ifndef OUTSHINE_WORLD_GROUND_STRUCTUREMESHER_H
#define OUTSHINE_WORLD_GROUND_STRUCTUREMESHER_H

#include <span>
#include <cstdint>
#include <vector>

#include "math/Vec3.h"

#include <generate/Generate.h>

#include "TileMeshes.h"

namespace outshine {

inline constexpr double kSteepestRoof = 0.5;

struct Raised {
  std::vector<TileVertex> WallCorners, RoofCorners;
  std::vector<uint32_t> WallRun, RoofRun;

  void Clear() noexcept {
    WallCorners.clear();
    RoofCorners.clear();
    WallRun.clear();
    RoofRun.clear();
  }

  void Settle() {
    WallCorners.shrink_to_fit();
    RoofCorners.shrink_to_fit();
    WallRun.shrink_to_fit();
    RoofRun.shrink_to_fit();
  }

  [[nodiscard]] std::size_t HeapBytes() const noexcept {
    return WallCorners.capacity() * sizeof(TileVertex) +
           RoofCorners.capacity() * sizeof(TileVertex) + WallRun.capacity() * sizeof(uint32_t) +
           RoofRun.capacity() * sizeof(uint32_t);
  }

  [[nodiscard]] std::size_t UsedBytes() const noexcept {
    return WallCorners.size() * sizeof(TileVertex) + RoofCorners.size() * sizeof(TileVertex) +
           WallRun.size() * sizeof(uint32_t) + RoofRun.size() * sizeof(uint32_t);
  }
};

struct WayLine {
  std::span<const double> LatLon;
  double HalfWidthM = 0.0;
  double MinLat = 0.0, MinLon = 0.0, MaxLat = 0.0, MaxLon = 0.0;
};

struct Frontage {
  bool Known = false;

  double KerbEm = 0.0, KerbNm = 0.0;
  double AlongE = 0.0, AlongN = 0.0;
  double ToStreetE = 0.0, ToStreetN = 0.0;
};

inline constexpr double kPitchedShareUnknown = -1.0;

struct StructurePlan {
  std::span<const double> RingLatLon;
  double BaseAslM = 0.0;

  double SeatAslM = 0.0;
  double FootAslM = 0.0;

  std::span<const double> CornerAslM;

  double HeightM = 0.0;
  bool HeightMeasured = false;
  Frontage Street;

  Vec3 AnchorEcef;

  double FocalPx = 0.0;

  Generators::Detail Coarseness = Generators::Detail::Fine;

  double PitchedShare = kPitchedShareUnknown;
};

class StructureMesher {
public:
  virtual ~StructureMesher() = default;
  StructureMesher(const StructureMesher &) = delete;
  StructureMesher &operator=(const StructureMesher &) = delete;

  [[nodiscard]] virtual bool Mesh(const StructurePlan &plan, Raised &into) const noexcept = 0;

protected:
  StructureMesher() = default;
};

} // namespace outshine
#endif

#ifndef OUTSHINE_WORLD_GROUND_STRUCTUREMESHER_H
#define OUTSHINE_WORLD_GROUND_STRUCTUREMESHER_H

#include <span>
#include <cstdint>
#include <vector>

#include "math/Vec3.h"

namespace outshine {

inline constexpr double kSteepestRoof = 0.5;

struct Raised {
  std::vector<float> WallCorners, RoofCorners;
  std::vector<uint32_t> WallRun, RoofRun;

  void Clear() noexcept {
    WallCorners.clear();
    RoofCorners.clear();
    WallRun.clear();
    RoofRun.clear();
  }

  [[nodiscard]] std::size_t HeapBytes() const noexcept {
    return WallCorners.capacity() * sizeof(float) + RoofCorners.capacity() * sizeof(float) +
           WallRun.capacity() * sizeof(uint32_t) + RoofRun.capacity() * sizeof(uint32_t);
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

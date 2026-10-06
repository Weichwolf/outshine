#include "BuildingPreparation.h"
#include "BuildingScratch.h"
#include "Geodesy.h"
#include "math/Units.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Generators {
namespace {

constexpr double kSinkM = 0.30;
constexpr double kPlinthM = 0.50;

class FoundationGround {
public:
  explicit FoundationGround(const StructurePlan &plan)
      : HighM_(plan.SeatAslM - plan.BaseAslM), LowM_(plan.FootAslM - plan.BaseAslM) {
    const auto ringLatLon = plan.RingLatLon;
    const auto cornerAslM = plan.CornerAslM;
    const double baseAslM = plan.BaseAslM;
    const size_t n = cornerAslM.size();
    if (n == 0) { return; }
    std::array<std::array<double, 4>, 3> m = {};
    for (size_t k = 0; k < n; k++) {
      const EastNorth away =
          EnuOffsetM({.LongitudeDeg = ringLatLon[1], .LatitudeDeg = ringLatLon[0]},
                     {.LongitudeDeg = ringLatLon[k * 2 + 1], .LatitudeDeg = ringLatLon[k * 2]});
      const double z = cornerAslM[k] - baseAslM;
      const Vec3 b = {{1.0, away.EastM, away.NorthM}};
      for (int r = 0; r < 3; r++) {
        for (int c = 0; c < 3; c++) { m[r][c] += b[r] * b[c]; }
        m[r][3] += b[r] * z;
      }
    }
    for (int c = 0; c < 3; c++) {
      int piv = c;
      for (int r = c + 1; r < 3; r++) {
        if (std::fabs(m[r][c]) > std::fabs(m[piv][c])) { piv = r; }
      }
      if (std::fabs(m[piv][c]) < kLeastRunM) { return; }
      for (int k = 0; k < 4; k++) { std::swap(m[c][k], m[piv][k]); }
      for (int r = 0; r < 3; r++) {
        if (r == c) { continue; }
        const double f = m[r][c] / m[c][c];
        for (int k = c; k < 4; k++) { m[r][k] -= f * m[c][k]; }
      }
    }
    Const_ = m[0][3] / m[0][0];
    SlopeE_ = m[1][3] / m[1][1];
    SlopeN_ = m[2][3] / m[2][2];
  }

  [[nodiscard]] double At(const EastNorth &p) const {
    return Const_ + SlopeE_ * p.EastM + SlopeN_ * p.NorthM;
  }

  [[nodiscard]] double High() const { return HighM_; }

  [[nodiscard]] double Low() const { return LowM_; }

private:
  double Const_ = 0.0, SlopeE_ = 0.0, SlopeN_ = 0.0;
  double HighM_ = 0.0, LowM_ = 0.0;
};

constexpr double kGroundStepM = 2.0;

void SampleGround(const BuildingShape &s,
                  const FoundationGround &ground,
                  double *lowest,
                  double *highest) {
  bool first = true;
  const size_t n = s.Ring.size();
  for (size_t i = 0; i < n; i++) {
    const EastNorth &p = s.Ring[i];
    const EastNorth &q = s.Ring[(i + 1) % n];
    const double len = std::hypot(q.EastM - p.EastM, q.NorthM - p.NorthM);
    const int steps = 1 + static_cast<int>(len / kGroundStepM);
    for (int step = 0; step < steps; ++step) {
      const double fraction = static_cast<double>(step) / static_cast<double>(steps);
      const double at = ground.At({.EastM = p.EastM + fraction * (q.EastM - p.EastM),
                                   .NorthM = p.NorthM + fraction * (q.NorthM - p.NorthM)});
      if (first) {
        *lowest = *highest = at;
        first = false;
        continue;
      }
      *lowest = std::min(at, *lowest);
      *highest = std::max(at, *highest);
    }
  }
  if (first) { *lowest = *highest = 0.0; }
}

double PlinthFootZ(const BuildingShape &s, const FoundationGround &ground) {
  double lowest = 0.0;
  double highest = 0.0;
  SampleGround(s, ground, &lowest, &highest);
  lowest = std::min(lowest, ground.Low());
  highest = std::max(highest, ground.High());
  const double spread = highest - lowest;
  return lowest - (spread > kSinkM ? 2.0 * spread : kSinkM);
}

double PlinthTopZ(const BuildingShape &s, const FoundationGround &ground) {
  double lowest = 0.0;
  double highest = 0.0;
  SampleGround(s, ground, &lowest, &highest);
  const double seatZ = ground.High();
  const double seat = std::max(seatZ, highest) + kPlinthM;

  return seat;
}

}

std::expected<std::span<BuildingShape>, StructureMeshError>
PrepareBuildingShapes(const StructurePlan &plan, BuildingScratch &scratch) {
  auto parts = MassOf(plan.RingLatLon,
                      {.HeightM = plan.HeightM,
                       .MinimumHeightM = plan.MinimumHeightM,
                       .HeightMeasured = plan.HeightMeasured,
                       .PitchedShare = plan.PitchedShare},
                      plan.Street,
                      scratch,
                      plan.InnerRings,
                      plan.RingPointsLatLon);
  if (!parts) { return std::unexpected(parts.error()); }
  const FoundationGround ground(plan);
  for (BuildingShape &part : *parts) {
    if (plan.WallColour) { part.WallVariant = 0; }
    part.SeatM = plan.MinimumHeightM != 0.0 ? 0.0 : PlinthTopZ(part, ground);
    part.SoleM = plan.MinimumHeightM != 0.0 ? plan.MinimumHeightM : PlinthFootZ(part, ground);
  }
  return parts;
}

double BuildingBottomM(const BuildingShape &shape, double minimumHeightM) noexcept {
  if (shape.OnGround()) { return shape.SoleM; }
  const double lower = shape.SeatM + shape.FootM - kSinkM;
  return minimumHeightM != 0.0 ? std::max(minimumHeightM, lower) : lower;
}

}

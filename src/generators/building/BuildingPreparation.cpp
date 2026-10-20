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
constexpr double kRegularTowerWidthM = 12.0;

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

void PlaceFoundation(BuildingShape &shape, const FoundationGround &ground) {
  double lowest = 0.0;
  double highest = 0.0;
  if (!shape.Ring.empty()) {
    lowest = highest = ground.At(shape.Ring.front());
    for (const EastNorth &corner : std::span(shape.Ring).subspan(1)) {
      const double height = ground.At(corner);
      lowest = std::min(lowest, height);
      highest = std::max(highest, height);
    }
  }
  shape.SeatM = std::max(ground.High(), highest) + kBuildingPlinthM;
  lowest = std::min(lowest, ground.Low());
  highest = std::max(highest, ground.High());
  const double spread = highest - lowest;
  shape.SoleM = lowest - (spread > kSinkM ? 2.0 * spread : kSinkM);
}

}

std::expected<std::span<BuildingShape>, StructureMeshError>
PrepareBuildingShapes(const StructurePlan &plan, BuildingScratch &scratch) {
  if (plan.Facade && !ValidFacadeStyle(*plan.Facade)) {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
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
    part.OpeningStyle = BuildingFacadeStyle(part.Form, plan.Facade);
    if (!plan.Facade && part.Form == BuildingForm::Tower &&
        2.0 * part.HalfVm >= kRegularTowerWidthM) {
      part.OpeningStyle = FacadeStyle::Block;
    }
    const auto &profile = FacadeProfileOf(part.OpeningStyle);
    if (part.EavesM > 0.0 && profile.OpeningWidth > 0.0f && profile.OpeningHeight > 0.0f) {
      part.Storeys = std::max(1, static_cast<int>(std::lround(part.EavesM / profile.FloorM)));
      part.FloorM = part.EavesM / static_cast<double>(part.Storeys);
      part.BayM = profile.BayM;
    }
    if (plan.WallColour) { part.WallVariant = 0; }
    if (plan.MinimumHeightM == 0.0) {
      PlaceFoundation(part, ground);
    } else {
      part.SeatM = 0.0;
      part.SoleM = plan.MinimumHeightM;
    }
  }
  return parts;
}

double BuildingBottomM(const BuildingShape &shape, double minimumHeightM) noexcept {
  if (shape.OnGround()) { return shape.SoleM; }
  const double lower = shape.SeatM + shape.FootM - kSinkM;
  return minimumHeightM != 0.0 ? std::max(minimumHeightM, lower) : lower;
}

}

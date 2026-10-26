#include "Check.h"
#include "TerrainFieldSet.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>

namespace {
using namespace outshine;
constexpr int kProbeIntervals = 24;
constexpr float kPeakM = 1500, kDepressionM = -400;
constexpr int kAncestorBaseM = -200, kAncestorStepM = 70;
constexpr size_t kNodesMost = 200;
constexpr float kRemotePeakM = 25000;

Ground::GeoBounds Bounds(int zoom, Ground::TileFrac first, Ground::TileFrac last) {
  const auto northWest = Ground::TileFracToGeo(first, zoom);
  const auto southEast = Ground::TileFracToGeo(last, zoom);
  return {.MinLonDeg = northWest.LongitudeDeg,
          .MinLatDeg = southEast.LatitudeDeg,
          .MaxLonDeg = southEast.LongitudeDeg,
          .MaxLatDeg = northWest.LatitudeDeg};
}

void CheckSamples(const Generators::TerrainFieldSet &fields,
                  Ground::GeoBounds bounds,
                  std::array<double, 2> range,
                  int zoom) {
  using namespace outshine::Test;
  for (int row = 0; row <= kProbeIntervals; ++row) {
    for (int column = 0; column <= kProbeIntervals; ++column) {
      const auto height = fields.AslMAt(
          zoom,
          {.LongitudeDeg = std::lerp(
               bounds.MinLonDeg, bounds.MaxLonDeg, column / static_cast<double>(kProbeIntervals)),
           .LatitudeDeg = std::lerp(
               bounds.MinLatDeg, bounds.MaxLatDeg, row / static_cast<double>(kProbeIntervals))});
      CHECK(height && *height >= range[0] && *height <= range[1],
            "each actual finest-available bilinear sample lies in the height interval");
    }
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Error = Generators::TerrainFieldSet::RangeError;
  const Data::TileId owner{.Zoom = 2, .X = 1, .Y = 1};
  auto terrain = std::make_shared<Ground::TerrainField>(9, 9);
  terrain->SetM(4, 4, kPeakM);
  terrain->SetM(4, 5, kDepressionM);
  Generators::TerrainFieldSet fields({{owner, terrain}});
  const auto bounds = Bounds(2, {.X = 1.2, .Y = 1.2}, {.X = 1.8, .Y = 1.8});
  const auto range = fields.AslRangeOver(2, bounds, 81);
  CHECK(range && range->Nodes == 81 && range->AslM[0] < kDepressionM && range->AslM[1] > kPeakM,
        "support nodes include the interior peak and depression missed by corner probes");
  if (range) {
    CheckSamples(fields, bounds, range->AslM, 2);
    const auto tight = fields.AslRangeOver(2, bounds, range->Nodes);
    const auto shortBudget = fields.AslRangeOver(2, bounds, range->Nodes - 1);
    CHECK(tight && !shortBudget && shortBudget.error() == Error::OverBudget,
          "the node limit is inclusive and a shorter budget refuses the query");
  }
  terrain->SetM(0, 0, kRemotePeakM);
  const auto local =
      fields.AslRangeOver(2, Bounds(2, {.X = 1.45, .Y = 1.45}, {.X = 1.55, .Y = 1.55}), 81);
  CHECK(local && local->Nodes < 81 && local->AslM[1] < kRemotePeakM,
        "a remote source peak outside the support halo does not inflate the selected interval");
  terrain->SetM(0, 0, 0);
  auto ancestor = std::make_shared<Ground::TerrainField>(9, 9);
  for (uint32_t row = 0; row < 9; ++row) {
    for (uint32_t column = 0; column < 9; ++column) {
      ancestor->SetM(
          row,
          column,
          static_cast<float>(kAncestorBaseM + kAncestorStepM * static_cast<int>(row + column)));
    }
  }
  fields = Generators::TerrainFieldSet({{owner, terrain}, {{.Zoom = 1, .X = 0, .Y = 0}, ancestor}});
  const auto seam = Bounds(2, {.X = .9, .Y = 1.3}, {.X = 1.7, .Y = 1.7});
  const auto mixed = fields.AslRangeOver(2, seam, kNodesMost);
  CHECK(mixed && mixed->Nodes < 162, "mixed finest and ancestor sources scan only ROI support");
  if (mixed) { CheckSamples(fields, seam, mixed->AslM, 2); }
  const auto empty =
      fields.AslRangeOver(2, Bounds(2, {.X = 2.1, .Y = 1.1}, {.X = 2.2, .Y = 1.2}), kNodesMost);
  CHECK(!empty && empty.error() == Error::MissingSource, "uncovered terrain remains unknown");
  terrain->SetM(4, 4, std::numeric_limits<float>::quiet_NaN());
  const auto nonFinite = fields.AslRangeOver(2, bounds, kNodesMost);
  CHECK(!nonFinite && nonFinite.error() == Error::IncompleteSource,
        "nonfinite support cannot become a numeric ground interval");
  terrain->SetM(4, 4, kPeakM);
  terrain->MarkMissingBoundary();
  const auto partial = fields.AslRangeOver(2, bounds, kNodesMost);
  CHECK(!partial && partial.error() == Error::IncompleteSource,
        "incomplete source boundaries do not qualify an interval");
  const auto overBudget = fields.AslRangeOver(30, bounds, kNodesMost);
  CHECK(!overBudget && overBudget.error() == Error::OverBudget,
        "huge geographic demand is refused before a tile traversal");
  for (const int zoom : {-1, 31}) {
    const auto invalid = fields.AslRangeOver(zoom, bounds, kNodesMost);
    CHECK(!invalid && invalid.error() == Error::InvalidRequest, "unsupported zoom is refused");
  }
  const Generators::TerrainFieldSet global({{{.Zoom = 0, .X = 0, .Y = 0}, ancestor}});
  for (const double longitude : {-179.9, 179.8}) {
    for (const double latitude : {-84.0, 83.9}) {
      const Ground::GeoBounds edge{.MinLonDeg = longitude,
                                   .MinLatDeg = latitude,
                                   .MaxLonDeg = longitude + .05,
                                   .MaxLatDeg = latitude + .05};
      const auto extremes = global.AslRangeOver(2, edge, kNodesMost);
      CHECK(extremes.has_value(), "both dateline sides and high latitudes retain ancestor support");
      if (extremes) { CheckSamples(global, edge, extremes->AslM, 2); }
    }
  }
  auto invalidBounds = bounds;
  invalidBounds.MinLonDeg = std::numeric_limits<double>::quiet_NaN();
  const auto invalid = global.AslRangeOver(2, invalidBounds, kNodesMost);
  const auto noBudget = global.AslRangeOver(2, bounds, 0);
  CHECK(!invalid && invalid.error() == Error::InvalidRequest && !noBudget &&
            noBudget.error() == Error::OverBudget,
        "invalid coordinates and zero work budget remain explicit refusals");
  return Report();
}

#include "Check.h"
#include "UrbanAreaInstances.h"
#include "Geodesy.h"
#include "math/RenderFrame.h"
#include "Units.h"
#include "Vec3.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = Generators::Osm;
constexpr double kAslM = 1200, kToleranceM = 1e-6;
constexpr uint32_t kFieldSide = 64;
constexpr size_t kSelectedLots = 3;
constexpr Osm::UrbanPatchRange kPatch{.Column = 16, .Row = 8, .Columns = 2, .Rows = 2};
constexpr size_t kFirst = static_cast<size_t>(kPatch.Row) * kFieldSide + kPatch.Column;
constexpr double kRiseM = 3, kCutM = 12;
constexpr std::array<std::optional<double>, 4> kGround{
    kAslM, kAslM + kRiseM, kAslM - kCutM, std::nullopt};

Osm::UrbanArea FieldAt(LongitudeLatitude at) {
  Osm::UrbanArea field;
  const double pitch = field.Recipe.ProjectedPitchM;
  field.FirstCell = {static_cast<int64_t>(std::floor(kWgs84A * at.LongitudeDeg * kDeg2Rad / pitch)),
                     static_cast<int64_t>(std::floor(
                         kWgs84A * std::asinh(std::tan(at.LatitudeDeg * kDeg2Rad)) / pitch))};
  field.Rows = field.Columns = kFieldSide;
  field.Tiers.assign(static_cast<size_t>(kFieldSide) * kFieldSide, 4);
  field.Tiers[kFirst] = 1;
  field.Tiers[kFirst + 1u] = 2;
  field.Tiers[kFirst + kFieldSide] = 3;
  field.Tiers[kFirst + kFieldSide + 1u] = 0;
  return field;
}

void CheckCorners(const Mat4 &model,
                  const Osm::UrbanLot &lot,
                  double aslM,
                  const TangentFrame &frame) {
  const LongitudeLatitudeHeight baseAt{.LongitudeDeg = Wrap180(lot.Centre.LongitudeDeg),
                                       .LatitudeDeg = lot.Centre.LatitudeDeg,
                                       .HeightM = aslM};
  Vec3 base;
  GeoToEcef(baseAt, base);
  const auto axes = EnuAxesEcef(baseAt);
  for (const double x : {-.5, .5}) {
    for (const double y : {0., 1.}) {
      for (const double z : {-.5, .5}) {
        const Vec3 expected{RenderFrame::Of(frame.ToLocalPosition(
            base + axes.East * (x * lot.EastSpanM) + axes.Up * (y * lot.HeightM) -
            axes.North * (z * lot.NorthSpanM)))};
        CHECK(Length(model.TransformPoint({{x, y, z}}) - expected) < kToleranceM,
              "box corners preserve independent ECEF placement, local gravity and physical scale");
      }
    }
  }
}

void CheckAt(LongitudeLatitude at) {
  const auto field = FieldAt(at);
  const auto frame = TangentFrame::At(
      {.LongitudeDeg = Wrap180(at.LongitudeDeg - 2), .LatitudeDeg = at.LatitudeDeg - 1});
  const auto prepared =
      PrepareUrbanPatchInstances(field,
                                 kPatch,
                                 UrbanPatchHeights{kGround.data(), kPatch.Rows, kPatch.Columns},
                                 frame,
                                 kSelectedLots);
  CHECK(prepared && prepared->size() == kSelectedLots,
        "only occupied cells of the selected patch consume the instance budget");
  if (!prepared || prepared->size() != kSelectedLots) { return; }
  size_t instance = 0;
  for (uint32_t row = 0; row < kPatch.Rows; ++row) {
    for (uint32_t column = 0; column < kPatch.Columns; ++column) {
      const auto lot = field.LotAt(kPatch.Column + column, kPatch.Row + row);
      if (lot) {
        const auto height = kGround[row * kPatch.Columns + column];
        CHECK(height.has_value(), "the independent control supplies every occupied lot height");
        if (!height) { return; }
        CheckCorners((*prepared)[instance++], *lot, *height, frame);
      }
    }
  }
}
}

int main() {
  const std::array<LongitudeLatitude, 5> sites{{{.LongitudeDeg = 0, .LatitudeDeg = 0},
                                                {.LongitudeDeg = 179.99999, .LatitudeDeg = 48},
                                                {.LongitudeDeg = -179.99999, .LatitudeDeg = -48},
                                                {.LongitudeDeg = 25, .LatitudeDeg = 84},
                                                {.LongitudeDeg = -140, .LatitudeDeg = -84}}};
  for (const auto site : sites) { CheckAt(site); }
  auto field = FieldAt(sites.front());
  const auto frame = TangentFrame::At(sites.front());
  const auto shortBudget = PrepareUrbanPatchInstances(field, kPatch, {}, frame, kSelectedLots - 1);
  CHECK(!shortBudget && shortBudget.error() == UrbanInstanceError::OverBudget,
        "a short budget refuses before any terrain work or matrix allocation");
  const auto missing = PrepareUrbanPatchInstances(field, kPatch, {}, frame, kSelectedLots);
  CHECK(!missing && missing.error() == UrbanInstanceError::MissingTerrain,
        "missing terrain cannot silently place a lot at sea level");
  auto invalidGround = kGround;
  invalidGround[0] = std::numeric_limits<double>::quiet_NaN();
  const auto nonfinite = PrepareUrbanPatchInstances(
      field,
      kPatch,
      UrbanPatchHeights{invalidGround.data(), kPatch.Rows, kPatch.Columns},
      frame,
      kSelectedLots);
  CHECK(!nonfinite && nonfinite.error() == UrbanInstanceError::MissingTerrain,
        "a nonfinite supplied surface height cannot enter a placement matrix");
  field.Tiers[kFirst] = field.Tiers[kFirst + 1u] = field.Tiers[kFirst + kFieldSide] = 0;
  const auto empty = PrepareUrbanPatchInstances(field, kPatch, {}, frame, 0);
  CHECK(empty && empty->empty(), "a known empty patch needs neither terrain nor output storage");
  auto invalid = kPatch;
  invalid.Columns = field.Recipe.PatchSide + 1;
  const auto tooWide =
      PrepareUrbanPatchInstances(field,
                                 invalid,
                                 UrbanPatchHeights{kGround.data(), kPatch.Rows, kPatch.Columns},
                                 frame,
                                 kSelectedLots);
  CHECK(!tooWide && tooWide.error() == UrbanInstanceError::InvalidRequest,
        "one preparation cannot expand beyond its intrinsic patch side");
  return Report();
}

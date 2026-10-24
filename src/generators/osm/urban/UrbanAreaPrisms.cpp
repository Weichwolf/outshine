#include "UrbanAreaPrisms.h"
#include "Geodesy.h"
#include "Units.h"
#include "Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
struct PatchFrame {
  PrismPatch Shape;
  Vec3 Centre;
  EnuAxes Axes;
  Mat4 EcefFromLocal;
};

std::expected<size_t, UrbanPrismError>
SampleCount(const UrbanArea &area, std::span<const UrbanPrismInput> patches, size_t most) {
  if (!area.Recipe.Valid() || area.Tiers.size() != uint64_t{area.Columns} * area.Rows) {
    return std::unexpected(UrbanPrismError::InvalidRequest);
  }
  size_t count = 0;
  for (const auto &input : patches) {
    const auto &p = input.Range;
    if (p.Columns == 0 || p.Rows == 0 || p.Column >= area.Columns || p.Row >= area.Rows ||
        p.Columns > area.Columns - p.Column || p.Rows > area.Rows - p.Row ||
        p.Columns > area.Recipe.PatchSide || p.Rows > area.Recipe.PatchSide) {
      return std::unexpected(UrbanPrismError::InvalidRequest);
    }
    const size_t cells = static_cast<size_t>(p.Columns) * p.Rows;
    if (cells > most - count) { return std::unexpected(UrbanPrismError::OverBudget); }
    count += cells;
  }
  return count;
}

std::expected<double, UrbanPrismError> ReferenceHeight(const UrbanArea &area,
                                                       const UrbanPrismInput &input) {
  const auto &p = input.Range;
  const auto heights = input.GroundAslM;
  double low = std::numeric_limits<double>::infinity();
  double high = -low;
  for (size_t row = 0; row < p.Rows; ++row) {
    for (size_t column = 0; column < p.Columns; ++column) {
      const auto tier = area.Tiers[(p.Row + row) * area.Columns + p.Column + column];
      if (tier >= UrbanArea::HeightsM.size()) {
        return std::unexpected(UrbanPrismError::InvalidRequest);
      }
      if (tier == 0) { continue; }
      if (heights.data_handle() == nullptr || heights.extent(0) != p.Rows ||
          heights.extent(1) != p.Columns) {
        return std::unexpected(UrbanPrismError::MissingTerrain);
      }
      const auto ground = heights[row, column];
      if (!ground || !std::isfinite(*ground)) {
        return std::unexpected(UrbanPrismError::MissingTerrain);
      }
      low = std::min(low, *ground);
      high = std::max(high, *ground);
    }
  }
  return std::isfinite(low) ? std::midpoint(low, high) : 0;
}

PatchFrame FrameFor(const UrbanArea &area, UrbanPatchRange p, double height) {
  const double pitch = area.Recipe.ProjectedPitchM;
  const double longitude =
      (static_cast<double>(area.FirstCell[0]) + p.Column + p.Columns * .5) * pitch / kWgs84A;
  const double latitude = std::atan(std::sinh(
      (static_cast<double>(area.FirstCell[1]) - p.Row + 1 - p.Rows * .5) * pitch / kWgs84A));
  const double sine = std::sin(latitude);
  const double cosine = std::cos(latitude);
  constexpr double e2 = kWgs84F * (2 - kWgs84F);
  const double d = 1 - e2 * sine * sine;
  const double prime = kWgs84A / std::sqrt(d);
  const LongitudeLatitudeHeight anchor{.LongitudeDeg = Wrap180(longitude / kDeg2Rad),
                                       .LatitudeDeg = latitude / kDeg2Rad,
                                       .HeightM = height};
  PatchFrame frame{
      .Shape = {.Columns = p.Columns,
                .Rows = p.Rows,
                .PitchX = static_cast<float>(pitch * cosine * (prime + height) / kWgs84A),
                .PitchZ =
                    static_cast<float>(pitch * cosine * (prime * (1 - e2) / d + height) / kWgs84A),
                .FillFraction = static_cast<float>(area.Recipe.FillFraction)},
      .Centre = {},
      .Axes = EnuAxesEcef(anchor),
      .EcefFromLocal = {}};
  GeoToEcef(anchor, frame.Centre);
  const Vec3 origin = frame.Centre -
                      frame.Axes.East * (static_cast<double>(p.Columns) * frame.Shape.PitchX * .5) +
                      frame.Axes.North * (static_cast<double>(p.Rows) * frame.Shape.PitchZ * .5);
  const std::array<Vec3, 4> columns{frame.Axes.East, frame.Axes.Up, frame.Axes.North * -1., origin};
  for (size_t axis = 0; axis < columns.size(); ++axis) {
    for (size_t component = 0; component < 3; ++component) {
      frame.EcefFromLocal.At(component, axis) = columns[axis][component];
    }
  }
  return frame;
}

double CornerError(const PatchFrame &frame,
                   const UrbanLot &lot,
                   Vec3 base,
                   PrismSpan sample,
                   size_t row,
                   size_t column) {
  const auto axes = EnuAxesEcef(
      {.LongitudeDeg = Wrap180(lot.Centre.LongitudeDeg), .LatitudeDeg = lot.Centre.LatitudeDeg});
  double error = 0;
  for (const double x : {-.5, .5}) {
    for (const double z : {-.5, .5}) {
      for (const bool top : {false, true}) {
        const Vec3 exact = base + axes.East * (x * lot.EastSpanM) -
                           axes.North * (z * lot.NorthSpanM) + axes.Up * (top ? lot.HeightM : 0.);
        const Vec3 local{
            {(static_cast<double>(column) + .5 + x * frame.Shape.FillFraction) * frame.Shape.PitchX,
             top ? sample.Upper : sample.Lower,
             (static_cast<double>(row) + .5 + z * frame.Shape.FillFraction) * frame.Shape.PitchZ}};
        error = std::max(error, Length(frame.EcefFromLocal.TransformPoint(local) - exact));
      }
    }
  }
  return error;
}

std::expected<double, UrbanPrismError> AppendSamples(const UrbanArea &area,
                                                     const UrbanPrismInput &input,
                                                     const PatchFrame &frame,
                                                     std::vector<PrismSpan> &samples) {
  double error = 0;
  for (uint32_t row = 0; row < input.Range.Rows; ++row) {
    for (uint32_t column = 0; column < input.Range.Columns; ++column) {
      const auto lot = area.LotAt(input.Range.Column + column, input.Range.Row + row);
      PrismSpan sample;
      if (lot) {
        const auto ground = input.GroundAslM[row, column];
        if (!ground) { return std::unexpected(UrbanPrismError::MissingTerrain); }
        Vec3 base;
        GeoToEcef({.LongitudeDeg = Wrap180(lot->Centre.LongitudeDeg),
                   .LatitudeDeg = lot->Centre.LatitudeDeg,
                   .HeightM = *ground},
                  base);
        const Vec3 delta = base - frame.Centre;
        const double lower = Dot(delta, frame.Axes.Up);
        sample = {.Lower = static_cast<float>(lower),
                  .Upper = static_cast<float>(lower + lot->HeightM)};
        if (!std::isfinite(sample.Lower) || !std::isfinite(sample.Upper) ||
            sample.Upper <= sample.Lower) {
          return std::unexpected(UrbanPrismError::Unrepresentable);
        }
        error = std::max(error, CornerError(frame, *lot, base, sample, row, column));
      }
      samples.push_back(sample);
    }
  }
  return error;
}
}

std::expected<UrbanPrisms, UrbanPrismError> PrepareUrbanPrisms(
    const UrbanArea &area, std::span<const UrbanPrismInput> patches, size_t samplesMost) {
  const auto count = SampleCount(area, patches, samplesMost);
  if (!count) { return std::unexpected(count.error()); }
  UrbanPrisms result;
  if (patches.empty()) { return result; }
  auto samples = std::make_shared<std::vector<PrismSpan>>();
  samples->reserve(*count);
  result.Geometry.Patches.reserve(patches.size());
  result.Placements.reserve(patches.size());
  for (const auto &input : patches) {
    const auto height = ReferenceHeight(area, input);
    if (!height) { return std::unexpected(height.error()); }
    auto frame = FrameFor(area, input.Range, *height);
    if (!std::isfinite(frame.Shape.PitchX) || frame.Shape.PitchX <= 0 ||
        !std::isfinite(frame.Shape.PitchZ) || frame.Shape.PitchZ <= 0 ||
        frame.Shape.FillFraction <= 0 ||
        !std::ranges::all_of(frame.EcefFromLocal, [](double x) { return std::isfinite(x); })) {
      return std::unexpected(UrbanPrismError::Unrepresentable);
    }
    frame.Shape.FirstSample = samples->size();
    const auto error = AppendSamples(area, input, frame, *samples);
    if (!error) { return std::unexpected(error.error()); }
    result.Geometry.Patches.push_back(frame.Shape);
    result.Placements.push_back({.EcefFromLocal = frame.EcefFromLocal, .MaxCornerErrorM = *error});
  }
  result.Geometry.Samples = std::move(samples);
  return result;
}
}

#include "SurfaceRibbon.h"
#include "GeosPolygon.h"
#include <geos_c.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <memory>
#include <span>
#include <stop_token>
#include <type_traits>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Error = SurfaceRibbonError;
using Ends = std::array<double, 2>;

PlanarPoint Difference(PlanarPoint a, PlanarPoint b) {
  return {a[0] - b[0], a[1] - b[1]};
}

double Dot(PlanarPoint a, PlanarPoint b) {
  return a[0] * b[0] + a[1] * b[1];
}

double Length(PlanarPoint a) {
  return std::hypot(a[0], a[1]);
}

std::expected<std::vector<double>, Error> Stations(std::span<const PlanarPoint> axis,
                                                   SurfaceRibbonBudget budget,
                                                   const std::stop_token &stop) {
  if (axis.size() > budget.AxisPoints || axis.size() >= std::numeric_limits<unsigned>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (axis.size() < 2) { return std::unexpected(Error::InvalidInput); }
  std::vector<double> stations;
  stations.reserve(axis.size());
  const double bound = std::sqrt(std::numeric_limits<double>::max()) / 8;
  for (size_t at = 0; at < axis.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    if (!std::isfinite(axis[at][0]) || !std::isfinite(axis[at][1]) ||
        std::abs(axis[at][0]) > bound || std::abs(axis[at][1]) > bound) {
      return std::unexpected(Error::InvalidInput);
    }
    const double length = at == 0 ? 0 : Length(Difference(axis[at], axis[at - 1]));
    const double station = at == 0 ? 0 : stations.back() + length;
    if (!std::isfinite(station) || (at != 0 && station <= stations.back())) {
      return std::unexpected(Error::InvalidInput);
    }
    stations.push_back(station);
  }
  return stations;
}

std::expected<Ends, Error> Offsets(std::span<const PlanarPoint> axis,
                                   std::span<const double> stations,
                                   SurfaceRibbonEnd end,
                                   SurfaceRibbonBudget budget,
                                   const std::stop_token &stop) {
  if (end.Profile.size() > budget.ProfilePoints) {
    return std::unexpected(Error::CapacityExceeded);
  }
  if (end.Profile.size() < 3 || end.AxisVertex == 0 || end.AxisVertex >= end.Profile.size() - 1 ||
      !std::isfinite(end.StationM) || end.StationM < 0 || end.StationM > stations.back()) {
    return std::unexpected(Error::InvalidProfile);
  }
  const auto upper = std::ranges::upper_bound(stations, end.StationM);
  const size_t segment =
      std::min(static_cast<size_t>(upper - stations.begin() - 1), axis.size() - 2);
  const auto delta = Difference(axis[segment + 1], axis[segment]);
  const double length = Length(delta);
  const PlanarPoint tangent{delta[0] / length, delta[1] / length};
  const PlanarPoint normal{-tangent[1], tangent[0]};
  const double distance = end.StationM - stations[segment];
  const PlanarPoint center{axis[segment][0] + tangent[0] * distance,
                           axis[segment][1] + tangent[1] * distance};
  if (Length(Difference(end.Profile[end.AxisVertex], center)) >
      SurfaceRibbonBudget::kMaximumProfileErrorM) {
    return std::unexpected(Error::InvalidProfile);
  }
  Ends offsets{};
  double previous = -std::numeric_limits<double>::infinity();
  for (size_t at = 0; at < end.Profile.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto relative = Difference(end.Profile[at], center);
    const double offset = Dot(relative, normal);
    if (!std::isfinite(offset) || !std::isfinite(Dot(relative, tangent)) ||
        std::abs(Dot(relative, tangent)) > SurfaceRibbonBudget::kMaximumProfileErrorM ||
        offset <= previous) {
      return std::unexpected(Error::InvalidProfile);
    }
    if (at == 0) { offsets[0] = offset; }
    previous = offset;
  }
  offsets[1] = previous;
  if (offsets[0] >= 0 || offsets[1] <= 0) { return std::unexpected(Error::InvalidProfile); }
  return offsets;
}

std::expected<void, Error> Shape(SurfaceRibbon &ribbon,
                                 SurfaceRibbonEnd first,
                                 SurfaceRibbonEnd last,
                                 std::span<const double> stations,
                                 Ends firstOffsets,
                                 Ends lastOffsets,
                                 const std::stop_token &stop) {
  std::vector<PlanarPoint> normals;
  normals.reserve(ribbon.Axis.size() - 1);
  for (size_t at = 1; at < ribbon.Axis.size(); ++at) {
    const auto delta = Difference(ribbon.Axis[at], ribbon.Axis[at - 1]);
    const double length = Length(delta);
    if (!std::isfinite(length) || length <= 0) { return std::unexpected(Error::InvalidInput); }
    normals.push_back({-delta[1] / length, delta[0] / length});
  }
  std::array<std::vector<PlanarPoint>, 2> sides;
  for (size_t at = 1; at + 1 < ribbon.Axis.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    PlanarPoint miter{(normals[at - 1][0] + normals[at][0]) / 2,
                      (normals[at - 1][1] + normals[at][1]) / 2};
    const double denominator = Dot(miter, normals[at - 1]);
    if (denominator <= SurfaceRibbonBudget::kMinimumMiterDenominator) {
      return std::unexpected(Error::UnsupportedTurn);
    }
    for (auto &value : miter) { value /= denominator; }
    ribbon.MaximumMiter = std::max(ribbon.MaximumMiter, Length(miter));
    const double fraction = (stations[at] - first.StationM) / (last.StationM - first.StationM);
    for (size_t side = 0; side < sides.size(); ++side) {
      const double offset = std::lerp(firstOffsets[side], lastOffsets[side], fraction);
      sides[side].push_back(
          {ribbon.Axis[at][0] + miter[0] * offset, ribbon.Axis[at][1] + miter[1] * offset});
    }
  }
  ribbon.MaximumMiter = std::max(1.0, ribbon.MaximumMiter);
  auto &ring = ribbon.Domain.Rings.emplace_back();
  ring.insert(ring.end(), first.Profile.begin(), first.Profile.end());
  ring.insert(ring.end(), sides[1].begin(), sides[1].end());
  ring.insert(ring.end(), last.Profile.rbegin(), last.Profile.rend());
  ring.insert(ring.end(), sides[0].rbegin(), sides[0].rend());
  return {};
}

std::expected<double, Error> ValidateDomain(const SurfaceRibbon &ribbon,
                                            const std::stop_token &stop) {
  const std::unique_ptr<std::remove_pointer_t<GEOSContextHandle_t>, decltype(&GEOS_finish_r)>
      context(GEOS_init_r(), GEOS_finish_r);
  if (!context) { return std::unexpected(Error::LibraryFailure); }
  const auto polygon = Spatial::MakeGeosPolygon(context.get(), ribbon.Domain.Rings);
  if (!polygon) { return std::unexpected(Error::LibraryFailure); }
  const char valid = GEOSisValid_r(context.get(), polygon.get());
  if (valid != 1) {
    return std::unexpected(valid == 0 ? Error::InvalidFootprint : Error::LibraryFailure);
  }
  auto *coordinates =
      GEOSCoordSeq_create_r(context.get(), static_cast<unsigned>(ribbon.Axis.size()), 2);
  if (coordinates == nullptr) { return std::unexpected(Error::LibraryFailure); }
  for (size_t at = 0; at < ribbon.Axis.size(); ++at) {
    if (GEOSCoordSeq_setXY_r(context.get(),
                             coordinates,
                             static_cast<unsigned>(at),
                             ribbon.Axis[at][0],
                             ribbon.Axis[at][1]) == 0) {
      GEOSCoordSeq_destroy_r(context.get(), coordinates);
      return std::unexpected(Error::LibraryFailure);
    }
  }
  const Spatial::GeosGeometry line(GEOSGeom_createLineString_r(context.get(), coordinates),
                                   {context.get()});
  if (!line) { return std::unexpected(Error::LibraryFailure); }
  const char covered = GEOSCovers_r(context.get(), polygon.get(), line.get());
  double area = 0;
  if (covered == 2 || GEOSArea_r(context.get(), polygon.get(), &area) == 0) {
    return std::unexpected(Error::LibraryFailure);
  }
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (covered != 1 || !std::isfinite(area) || area <= 0) {
    return std::unexpected(Error::InvalidFootprint);
  }
  return area;
}
}

std::expected<SurfaceRibbon, SurfaceRibbonError>
BuildSurfaceRibbon(std::span<const PlanarPoint> originalAxis,
                   SurfaceRibbonEnd first,
                   SurfaceRibbonEnd last,
                   SurfaceRibbonBudget budget,
                   const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto stations = Stations(originalAxis, budget, stop);
  if (!stations) { return std::unexpected(stations.error()); }
  const auto start = Offsets(originalAxis, *stations, first, budget, stop);
  if (!start) { return std::unexpected(start.error()); }
  const auto end = Offsets(originalAxis, *stations, last, budget, stop);
  if (!end) { return std::unexpected(end.error()); }
  if (first.StationM >= last.StationM) { return std::unexpected(Error::InvalidInput); }
  SurfaceRibbon ribbon;
  std::vector<double> croppedStations{first.StationM};
  ribbon.Axis.push_back(first.Profile[first.AxisVertex]);
  for (size_t at = 1; at + 1 < originalAxis.size(); ++at) {
    if ((*stations)[at] <= first.StationM || (*stations)[at] >= last.StationM) { continue; }
    ribbon.Axis.push_back(originalAxis[at]);
    croppedStations.push_back((*stations)[at]);
  }
  ribbon.Axis.push_back(last.Profile[last.AxisVertex]);
  croppedStations.push_back(last.StationM);
  const size_t ringPoints =
      first.Profile.size() + last.Profile.size() + 2 * (ribbon.Axis.size() - 2);
  if (ringPoints > budget.RingPoints || ringPoints >= std::numeric_limits<unsigned>::max()) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const auto shape = Shape(ribbon, first, last, croppedStations, *start, *end, stop);
  if (!shape) { return std::unexpected(shape.error()); }
  const auto area = ValidateDomain(ribbon, stop);
  if (!area) { return std::unexpected(area.error()); }
  ribbon.AreaM2 = *area;
  return ribbon;
}
}

#include "BuildingRoofProxy.h"
#include "BuildingShape.h"
#include "BuildingSurface.h"
#include "Geodesy.h"
#include "Vec3.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace outshine::Generators {
namespace {
constexpr double kRoofEdgeRoundoffM = 1e-8;

std::optional<double> Orientation(const BuildingShape &shape) {
  const auto &ring = shape.Ring;
  double orientation = 0;
  for (size_t i = 0; i < ring.size(); ++i) {
    const auto &a = ring[i];
    const auto &b = ring[(i + 1) % ring.size()];
    const auto &c = ring[(i + 2) % ring.size()];
    const double turn =
        (b.EastM - a.EastM) * (c.NorthM - b.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - b.EastM);
    if (!std::isfinite(turn) || turn == 0 || (i != 0 && std::signbit(turn) != (orientation < 0))) {
      return std::nullopt;
    }
    if (i == 0) { orientation = std::copysign(1.0, turn); }
  }
  return orientation;
}

bool FitsRoof(const BuildingShape &shape) {
  if (shape.Roof == RoofKind::Flat) { return true; }
  if (shape.Roof != RoofKind::Gable && shape.Roof != RoofKind::Shed) { return false; }
  return shape.HalfVm > 0 && std::ranges::all_of(shape.Ring, [&](const EastNorth &point) {
           const double v = shape.ToBox(point).V;
           return std::abs(v) <= shape.HalfVm + kRoofEdgeRoundoffM;
         });
}

BuildingRoofProxy::Plane
PlaneIn(const EnuAxes &axes, const Vec3 &centre, const Vec3 &normal, double offset) {
  const double inverseLength = 1.0 / Length(normal);
  const Vec3 direction =
      (axes.East * normal[0] + axes.North * normal[1] + axes.Up * normal[2]) * inverseLength;
  return {static_cast<float>(direction[0]),
          static_cast<float>(direction[1]),
          static_cast<float>(direction[2]),
          static_cast<float>((offset - Dot(normal, centre)) * inverseLength)};
}

void WriteRoof(const BuildingShape &shape,
               const EnuAxes &axes,
               const Vec3 &centre,
               BuildingRoofProxy &record) {
  const double eaves = shape.SeatM + shape.FootM + shape.EavesM;
  if (shape.Roof == RoofKind::Flat) {
    record.Planes[5] = PlaneIn(axes, centre, {{0, 0, 1}}, eaves + shape.RiseM);
    return;
  }
  const auto across = shape.AxisV();
  const bool shed = shape.Roof == RoofKind::Shed;
  const double slope = shape.RiseM / shape.HalfVm * (shed ? -0.5 : 1.0);
  const double shift =
      slope * (across.EastM * shape.Centre.EastM + across.NorthM * shape.Centre.NorthM);
  const double top = eaves + shape.RiseM * (shed ? 0.5 : 1.0);
  const Vec3 normal{{across.EastM * slope, across.NorthM * slope, 1}};
  record.Planes[5] = PlaneIn(axes, centre, normal, top + shift);
  if (!shed) {
    record.Planes[6] = PlaneIn(axes, centre, {{-normal[0], -normal[1], 1}}, top - shift);
  }
}
}

std::optional<BuildingRoofProxy> BuildingRoofProxy::Of(const BuildingSurface &surface,
                                                       uint32_t source) {
  const auto shapes = surface.Shapes();
  if (shapes.size() != 1 || surface.Bounds().Empty()) { return std::nullopt; }
  const auto &shape = shapes.front();
  if (shape.Ring.size() != 4 || !shape.Holes.empty() || !shape.OnGround() || shape.RoundFootprint ||
      shape.RiseM < 0 || shape.SoleM >= shape.SeatM + shape.EavesM || !FitsRoof(shape)) {
    return std::nullopt;
  }
  const auto orientation = Orientation(shape);
  if (!orientation) { return std::nullopt; }
  const auto &bounds = surface.Bounds();
  BuildingRoofProxy record{.Source = source, .Bounds = {}, .Planes = {}};
  constexpr float infinity = std::numeric_limits<float>::infinity();
  for (size_t i = 0; i < 3; ++i) {
    record.Bounds.Min[i] = std::nextafter(static_cast<float>(bounds.Min[i]), -infinity);
    record.Bounds.Max[i] = std::nextafter(static_cast<float>(bounds.Max[i]), infinity);
  }
  const auto &axes = surface.Axes();
  const Vec3 relative = record.WiderBounds().Middle() - surface.Origin();
  const Vec3 centre{{Dot(relative, axes.East), Dot(relative, axes.North), Dot(relative, axes.Up)}};
  for (size_t i = 0; i < shape.Ring.size(); ++i) {
    const auto &a = shape.Ring[i];
    const auto &b = shape.Ring[(i + 1) % shape.Ring.size()];
    const Vec3 normal{
        {*orientation * (b.NorthM - a.NorthM), -*orientation * (b.EastM - a.EastM), 0}};
    record.Planes[i] = PlaneIn(axes, centre, normal, normal[0] * a.EastM + normal[1] * a.NorthM);
  }
  record.Planes[4] = PlaneIn(axes, centre, {{0, 0, -1}}, -shape.SoleM);
  record.Planes[6] = {0, 0, 0, 1};
  WriteRoof(shape, axes, centre, record);
  if (!record.Valid()) { return std::nullopt; }
  return record;
}

Box BuildingRoofProxy::WiderBounds() const noexcept {
  Box result;
  for (size_t i = 0; i < 3; ++i) {
    result.Min[i] = Bounds.Min[i];
    result.Max[i] = Bounds.Max[i];
  }
  return result;
}

double BuildingRoofProxy::DiameterM() const noexcept {
  return std::nextafter(Length(WiderBounds().Span()), std::numeric_limits<double>::infinity());
}

bool BuildingRoofProxy::Valid() const noexcept {
  return Bounds.Holds(Bounds.Max) &&
         std::ranges::all_of(Bounds.Min, [](float x) { return std::isfinite(x); }) &&
         std::ranges::all_of(Bounds.Max, [](float x) { return std::isfinite(x); }) &&
         std::ranges::all_of(Planes, [](const Plane &plane) {
           return std::ranges::all_of(plane, [](float x) { return std::isfinite(x); });
         });
}
}

#include <algorithm>
#include <expected>
#include <span>
#include <string_view>
#include "math/RenderFrame.h"
#include "math/Vec2.h"
#include "Carriageway.h"
#include "Ribbon.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <string>
#include <cstdint>

namespace outshine {

namespace {

constexpr size_t kMaxRibbonStations = 262144;
constexpr size_t kVerticesPerStation = kRibbonAcross * 2 + 4;

void Put(std::vector<float> &into, double a, double b, double c) {
  into.push_back(static_cast<float>(a));
  into.push_back(static_cast<float>(b));
  into.push_back(static_cast<float>(c));
}

void WallsAt(const Placed &on,
             const Vec2 &left,
             const std::array<Astride, kRibbonAcross> &stood,
             const std::array<double, kRibbonAcross> &acrossAt,
             double thicknessM,
             Ribbon &out) {
  for (const size_t which : {size_t{0}, kRibbonAcross - 1u}) {
    const Astride &surface = stood[which];
    const double outward = which == 0 ? -1.0 : 1.0;
    const double eastM = on.EastM + left[0] * acrossAt[which];
    const double northM = on.NorthM + left[1] * acrossAt[which];
    for (const bool below : {false, true}) {
      const double sink = below ? thicknessM : 0.0;
      Put(out.PositionM,
          eastM - surface.NormalM[0] * sink - out.OriginM[0],
          surface.HeightM - surface.NormalM[1] * sink - out.OriginM[1],
          RenderFrame::ZOfNorth(northM - surface.NormalM[2] * sink) - out.OriginM[2]);
      Put(out.NormalM, left[0] * outward, 0.0, RenderFrame::ZOfNorth(left[1] * outward));
      out.AcrossM.push_back(static_cast<float>(acrossAt[which]));
    }
  }
}

[[nodiscard]] std::expected<size_t, std::string_view>
StationCount(const Section &section, double fromM, double toM, double stepM) {
  if (!std::isfinite(section.HalfWidthM) || !(section.HalfWidthM > 0.0) ||
      !std::isfinite(section.ThicknessM) || !(section.ThicknessM > 0.0) ||
      !std::isfinite(section.ShoulderM) || section.ShoulderM < 0.0 ||
      !std::isfinite(section.HalfWidthM + section.ShoulderM)) {
    return std::unexpected("ribbon width and thickness must be finite and positive; shoulder must "
                           "be finite and nonnegative");
  }
  if (!std::isfinite(fromM) || !std::isfinite(toM) || !std::isfinite(stepM) || !(stepM > 0.0) ||
      !(toM > fromM)) {
    return std::unexpected(
        "ribbon interval must be finite and forward with a finite positive step");
  }
  const double intervals = (toM - fromM) / stepM;
  if (!std::isfinite(intervals) || intervals >= static_cast<double>(kMaxRibbonStations)) {
    return std::unexpected("ribbon station count exceeds its generation budget");
  }
  const auto whole = static_cast<size_t>(intervals);
  const bool onGrid = fromM + static_cast<double>(whole) * stepM == toM;
  const size_t stations = whole + (onGrid ? 1 : 2);
  if (stations > kMaxRibbonStations) {
    return std::unexpected("ribbon station count exceeds its generation budget");
  }
  return stations;
}

void ConnectStations(size_t stations, std::span<const uint32_t> bands, Ribbon &out) {
  const auto perStation = static_cast<uint32_t>(kVerticesPerStation);
  for (size_t station = 0; station + 1 < stations; ++station) {
    const uint32_t here = static_cast<uint32_t>(station) * perStation;
    const uint32_t next = here + perStation;
    for (const uint32_t which : bands) {
      out.Index.insert(out.Index.end(),
                       {here + which,
                        next + which,
                        here + which + 1,
                        here + which + 1,
                        next + which,
                        next + which + 1});
      const auto under = static_cast<uint32_t>(kRibbonAcross);
      out.Index.insert(out.Index.end(),
                       {here + under + which,
                        here + under + which + 1,
                        next + under + which,
                        here + under + which + 1,
                        next + under + which + 1,
                        next + under + which});
    }
    const auto wall = static_cast<uint32_t>(kRibbonAcross * 2);
    const uint32_t leftTop = here + wall;
    const uint32_t leftBottom = here + wall + 1u;
    const uint32_t rightTop = here + wall + 2u;
    const uint32_t rightBottom = here + wall + 3u;
    const uint32_t nextLeftTop = next + wall;
    const uint32_t nextLeftBottom = next + wall + 1u;
    const uint32_t nextRightTop = next + wall + 2u;
    const uint32_t nextRightBottom = next + wall + 3u;
    out.Index.insert(out.Index.end(),
                     {leftTop, leftBottom, nextLeftTop, nextLeftTop, leftBottom, nextLeftBottom});
    out.Index.insert(
        out.Index.end(),
        {rightTop, nextRightTop, rightBottom, nextRightTop, nextRightBottom, rightBottom});
  }
}

void CloseEnds(const ReferenceLine &along,
               size_t stations,
               std::span<const uint32_t> bands,
               Ribbon &out) {
  for (const bool atEnd : {false, true}) {
    const double atM = atEnd ? out.ToM : out.FromM;
    Placed on;
    if (!along.At(atM, on)) { continue; }
    const double outward = atEnd ? 1.0 : -1.0;
    const double aheadE = std::cos(on.HeadingRad) * outward;
    const double aheadN = std::sin(on.HeadingRad) * outward;
    const auto base = static_cast<uint32_t>(out.PositionM.size() / 3);
    const uint32_t ring = atEnd ? static_cast<uint32_t>((stations - 1) * kVerticesPerStation) : 0u;
    for (size_t which = 0; which < kRibbonAcross * 2; ++which) {
      const size_t from = (static_cast<size_t>(ring) + which) * 3;
      Put(out.PositionM, out.PositionM[from], out.PositionM[from + 1], out.PositionM[from + 2]);
      Put(out.NormalM, aheadE, 0.0, RenderFrame::ZOfNorth(aheadN));
      out.AcrossM.push_back(out.AcrossM[static_cast<size_t>(ring) + which]);
    }
    const auto under = static_cast<uint32_t>(kRibbonAcross);
    for (const uint32_t which : bands) {
      const uint32_t topA = base + which;
      const uint32_t topB = base + which + 1;
      const uint32_t botA = base + under + which;
      const uint32_t botB = base + under + which + 1;
      if (atEnd) {
        out.Index.insert(out.Index.end(), {topA, botA, topB, topB, botA, botB});
      } else {
        out.Index.insert(out.Index.end(), {topA, topB, botA, topB, botB, botA});
      }
    }
  }
}

void BoundaryNormals(size_t stations, Ribbon &out) {
  const size_t bodyVertices = stations * kVerticesPerStation;
  const size_t surfaceVertices = stations * kRibbonAcross * 2;
  const auto boundaryIndex = [=](size_t vertex) {
    return vertex < bodyVertices
               ? vertex / kVerticesPerStation * 4 + vertex % kVerticesPerStation - kRibbonAcross * 2
               : vertex - surfaceVertices;
  };
  std::vector<Vec3> summed(out.PositionM.size() / 3 - surfaceVertices);
  const auto point = [&](size_t vertex) {
    const size_t at = vertex * 3;
    return Vec3{{out.PositionM[at], out.PositionM[at + 1], out.PositionM[at + 2]}};
  };
  for (size_t at = 0; at + 2 < out.Index.size(); at += 3) {
    const size_t first = out.Index[at];
    if (first < bodyVertices && first % kVerticesPerStation < kRibbonAcross * 2) { continue; }
    const Vec3 a = point(first);
    const Vec3 b = point(out.Index[at + 1]);
    const Vec3 c = point(out.Index[at + 2]);
    Vec3 ab;
    Vec3 ac;
    for (size_t axis = 0; axis < 3; ++axis) {
      ab[axis] = b[axis] - a[axis];
      ac[axis] = c[axis] - a[axis];
    }
    const Vec3 normal = Cross(ab, ac);
    for (size_t corner = 0; corner < 3; ++corner) {
      Vec3 &sum = summed[boundaryIndex(out.Index[at + corner])];
      for (size_t axis = 0; axis < 3; ++axis) { sum[axis] += normal[axis]; }
    }
  }
  for (size_t at = 0; at < summed.size(); ++at) {
    const Vec3 &normal = summed[at];
    const double length = std::hypot(normal[0], normal[1], normal[2]);
    if (!(length > 0.0)) { continue; }
    const size_t vertex = at < stations * 4
                              ? at / 4 * kVerticesPerStation + kRibbonAcross * 2 + at % 4
                              : at + surfaceVertices;
    for (size_t axis = 0; axis < 3; ++axis) {
      out.NormalM[vertex * 3 + axis] = static_cast<float>(normal[axis] / length);
    }
  }
}

}

Ribbon
Sweep(const ReferenceLine &along, const Section &section, double fromM, double toM, double stepM) {
  Ribbon out;
  out.FromM = fromM;
  out.ToM = toM;

  const auto count = StationCount(section, fromM, toM, stepM);
  if (!count) {
    out.Error = count.error();
    return out;
  }
  const size_t stations = *count;

  const std::array<double, kRibbonAcross> acrossAt = {{-(section.HalfWidthM + section.ShoulderM),
                                                       -section.HalfWidthM,
                                                       section.HalfWidthM,
                                                       section.HalfWidthM + section.ShoulderM}};

  const size_t vertices = stations * kVerticesPerStation + 4 * kRibbonAcross;
  out.PositionM.reserve(vertices * 3);
  out.NormalM.reserve(vertices * 3);
  out.AcrossM.reserve(vertices);
  out.Index.reserve((stations - 1) * kRibbonAcross * 12 + (kRibbonAcross - 1) * 12);

  {
    Placed first;
    if (!along.At(fromM, first)) {
      out.Error = "the reference line places nothing at " + std::to_string(fromM) + " m";
      return out;
    }
    const Astride surface = StandAt(along, {.AlongM = fromM, .AcrossM = 0.0, .HalfWidthM = 0.0});
    out.OriginM[0] = first.EastM;
    out.OriginM[1] = surface.HeightM;
    out.OriginM[2] = RenderFrame::ZOfNorth(first.NorthM);
  }

  for (size_t station = 0; station < stations; ++station) {
    const double atM = station + 1 == stations ? toM : fromM + static_cast<double>(station) * stepM;
    Placed on;
    if (!along.At(atM > toM ? toM : atM, on)) {
      out.Error = "the reference line places nothing at " + std::to_string(atM) + " m";
      return out;
    }
    const Vec2 left = {{-std::sin(on.HeadingRad), std::cos(on.HeadingRad)}};

    std::array<Astride, kRibbonAcross> stood{};
    for (size_t which = 0; which < kRibbonAcross; ++which) {
      stood[which] = StandAt(
          along, {.AlongM = atM > toM ? toM : atM, .AcrossM = acrossAt[which], .HalfWidthM = 0.0});
      const double eastM = on.EastM + left[0] * acrossAt[which];
      const double northM = on.NorthM + left[1] * acrossAt[which];
      Put(out.PositionM,
          eastM - out.OriginM[0],
          stood[which].HeightM - out.OriginM[1],
          RenderFrame::ZOfNorth(northM) - out.OriginM[2]);
      Put(out.NormalM,
          stood[which].NormalM[0],
          stood[which].NormalM[1],
          RenderFrame::ZOfNorth(stood[which].NormalM[2]));
      out.AcrossM.push_back(static_cast<float>(acrossAt[which]));
    }
    for (size_t which = 0; which < kRibbonAcross; ++which) {
      const Astride &surface = stood[which];
      const double eastM = on.EastM + left[0] * acrossAt[which];
      const double northM = on.NorthM + left[1] * acrossAt[which];
      Put(out.PositionM,
          eastM - surface.NormalM[0] * section.ThicknessM - out.OriginM[0],
          surface.HeightM - surface.NormalM[1] * section.ThicknessM - out.OriginM[1],
          RenderFrame::ZOfNorth(northM - surface.NormalM[2] * section.ThicknessM) - out.OriginM[2]);
      Put(out.NormalM, -surface.NormalM[0], -surface.NormalM[1], surface.NormalM[2]);
      out.AcrossM.push_back(static_cast<float>(acrossAt[which]));
    }
    WallsAt(on, left, stood, acrossAt, section.ThicknessM, out);
  }

  constexpr std::array<uint32_t, 3> allBands = {0, 1, 2};
  const std::span<const uint32_t> bands = allBands;
  const auto activeBands = section.ShoulderM == 0.0 ? bands.subspan(1, 1) : bands;
  ConnectStations(stations, activeBands, out);
  CloseEnds(along, stations, activeBands, out);
  BoundaryNormals(stations, out);

  out.Stations = stations;
  out.Vertices = out.PositionM.size() / 3;
  out.Triangles = out.Index.size() / 3;
  out.Woven = true;
  return out;
}

}

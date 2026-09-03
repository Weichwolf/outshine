#include <algorithm>
#include "math/Vec2.h"
#include "Ribbon.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <string>
#include <cstdint>

#include "Carriageway.h"

namespace outshine {

namespace {

constexpr size_t kMaxRibbonStations = 262144;

void Put(std::vector<float> &into, double a, double b, double c) {
  into.push_back(static_cast<float>(a));
  into.push_back(static_cast<float>(b));
  into.push_back(static_cast<float>(c));
}

} // namespace

Ribbon
Sweep(const ReferenceLine &along, const Section &section, double fromM, double toM, double stepM) {
  Ribbon out;
  out.FromM = fromM;
  out.ToM = toM;

  if (!(section.HalfWidthM > 0.0)) {
    out.Error = "a ribbon is swept from a carriageway with a width, and this section declares " +
                std::to_string(section.HalfWidthM) + " m";
    return out;
  }
  if (!(section.ThicknessM > 0.0)) {
    out.Error = "a road has a thickness, and this section declares " +
                std::to_string(section.ThicknessM) +
                " m -- a surface has no soffit for anything to pass under";
    return out;
  }
  if (!(stepM > 0.0) || !(toM > fromM)) {
    out.Error = "a ribbon is swept forwards at a positive step, and this one asks for " +
                std::to_string(toM - fromM) + " m at " + std::to_string(stepM);
    return out;
  }

  const auto whole = static_cast<size_t>((toM - fromM) / stepM);
  const bool onGrid = fromM + static_cast<double>(whole) * stepM == toM;
  const size_t stations = whole + (onGrid ? 1 : 2);
  if (stations > kMaxRibbonStations) {
    out.Error = "a ribbon of " + std::to_string(stations) + " stations reaches the bound of " +
                std::to_string(kMaxRibbonStations);
    return out;
  }

  const std::array<double, kRibbonAcross> acrossAt = {{-(section.HalfWidthM + section.ShoulderM),
                                                       -section.HalfWidthM,
                                                       section.HalfWidthM,
                                                       section.HalfWidthM + section.ShoulderM}};

  out.PositionM.reserve((stations + 2) * kRibbonAcross * 2 * 3);
  out.NormalM.reserve((stations + 2) * kRibbonAcross * 2 * 3);
  out.AcrossM.reserve((stations + 2) * kRibbonAcross * 2);
  out.Index.reserve((stations - 1) * kRibbonAcross * 12 + (kRibbonAcross - 1) * 12);

  {
    Placed first;
    if (!along.At(fromM > toM ? toM : fromM, first)) {
      out.Error = "the reference line places nothing at " + std::to_string(fromM) + " m";
      return out;
    }
    const Astride surface =
        StandAt(along, {.AlongM = fromM > toM ? toM : fromM, .AcrossM = 0.0, .HalfWidthM = 0.0});
    out.OriginM[0] = first.EastM;
    out.OriginM[1] = surface.HeightM;
    out.OriginM[2] = -first.NorthM;
  }

  for (size_t station = 0; station < stations; ++station) {
    const double atM = fromM + static_cast<double>(station) * stepM;
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
          -northM - out.OriginM[2]);
      Put(out.NormalM, stood[which].NormalM[0], stood[which].NormalM[1], -stood[which].NormalM[2]);
      out.AcrossM.push_back(static_cast<float>(acrossAt[which]));
    }
    for (size_t which = 0; which < kRibbonAcross; ++which) {
      const Astride &surface = stood[which];
      const double eastM = on.EastM + left[0] * acrossAt[which];
      const double northM = on.NorthM + left[1] * acrossAt[which];
      Put(out.PositionM,
          eastM - surface.NormalM[0] * section.ThicknessM - out.OriginM[0],
          surface.HeightM - surface.NormalM[1] * section.ThicknessM - out.OriginM[1],
          -(northM - surface.NormalM[2] * section.ThicknessM) - out.OriginM[2]);
      Put(out.NormalM, -surface.NormalM[0], -surface.NormalM[1], surface.NormalM[2]);
      out.AcrossM.push_back(static_cast<float>(acrossAt[which]));
    }
  }

  const auto perStation = static_cast<uint32_t>(kRibbonAcross * 2);
  for (size_t station = 0; station + 1 < stations; ++station) {
    const uint32_t here = static_cast<uint32_t>(station) * perStation;
    const uint32_t next = here + perStation;
    for (uint32_t which = 0; which + 1 < static_cast<uint32_t>(kRibbonAcross); ++which) {
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
    const auto under = static_cast<uint32_t>(kRibbonAcross);
    const uint32_t edge = static_cast<uint32_t>(kRibbonAcross) - 1;
    out.Index.insert(out.Index.end(), {here, here + under, next, next, here + under, next + under});
    out.Index.insert(out.Index.end(),
                     {here + edge,
                      next + edge,
                      here + under + edge,
                      next + edge,
                      next + under + edge,
                      here + under + edge});
  }

  for (const bool atEnd : {false, true}) {
    const double atM =
        atEnd ? std::min(fromM + static_cast<double>(stations - 1) * stepM, toM) : fromM;
    Placed on;
    if (!along.At(atM, on)) { continue; }
    const double outward = atEnd ? 1.0 : -1.0;
    const double aheadE = std::cos(on.HeadingRad) * outward;
    const double aheadN = std::sin(on.HeadingRad) * outward;
    const auto base = static_cast<uint32_t>(out.PositionM.size() / 3);
    const uint32_t ring = atEnd ? static_cast<uint32_t>((stations - 1) * kRibbonAcross * 2) : 0u;
    for (size_t which = 0; which < kRibbonAcross * 2; ++which) {
      const size_t from = (static_cast<size_t>(ring) + which) * 3;
      Put(out.PositionM, out.PositionM[from], out.PositionM[from + 1], out.PositionM[from + 2]);
      Put(out.NormalM, aheadE, 0.0, -aheadN);
      out.AcrossM.push_back(out.AcrossM[static_cast<size_t>(ring) + which]);
    }
    const auto under = static_cast<uint32_t>(kRibbonAcross);
    for (uint32_t which = 0; which + 1 < static_cast<uint32_t>(kRibbonAcross); ++which) {
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

  out.Stations = stations;
  out.Vertices = out.PositionM.size() / 3;
  out.Triangles = out.Index.size() / 3;
  out.Woven = true;
  return out;
}

} // namespace outshine

#include "Ribbon.h"

#include <cmath>

#include "Carriageway.h"

namespace outshine {

namespace {

constexpr size_t kMaxRibbonStations = 262144;

void Put(std::vector<float> &into, double a, double b, double c) {
  into.push_back((float)a);
  into.push_back((float)b);
  into.push_back((float)c);
}

} // namespace

Ribbon Sweep(const ReferenceLine &along, const Section &section, double fromM, double toM,
             double stepM) {
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

  const size_t stations = (size_t)((toM - fromM) / stepM) + 1;
  if (stations > kMaxRibbonStations) {
    out.Error = "a ribbon of " + std::to_string(stations) + " stations reaches the bound of " +
                std::to_string(kMaxRibbonStations);
    return out;
  }

  const double acrossAt[kRibbonAcross] = {
      -(section.HalfWidthM + section.ShoulderM), -section.HalfWidthM, section.HalfWidthM,
      section.HalfWidthM + section.ShoulderM};

  out.PositionM.reserve(stations * kRibbonAcross * 2 * 3);
  out.NormalM.reserve(stations * kRibbonAcross * 2 * 3);
  out.AcrossM.reserve(stations * kRibbonAcross * 2);

  for (size_t station = 0; station < stations; ++station) {
    const double atM = fromM + (double)station * stepM;
    Placed on;
    if (!along.At(atM > toM ? toM : atM, on)) {
      out.Error = "the reference line places nothing at " + std::to_string(atM) + " m";
      return out;
    }
    const double left[2] = {-std::sin(on.HeadingRad), std::cos(on.HeadingRad)};

    for (size_t which = 0; which < kRibbonAcross; ++which) {
      const Standing surface = StandAt(along, atM > toM ? toM : atM, acrossAt[which], 0.0);
      const double eastM = on.EastM + left[0] * acrossAt[which];
      const double northM = on.NorthM + left[1] * acrossAt[which];
      Put(out.PositionM, eastM, surface.HeightM, -northM);
      Put(out.NormalM, surface.NormalM[0], surface.NormalM[1], -surface.NormalM[2]);
      out.AcrossM.push_back((float)acrossAt[which]);
    }
    for (size_t which = 0; which < kRibbonAcross; ++which) {
      const Standing surface = StandAt(along, atM > toM ? toM : atM, acrossAt[which], 0.0);
      const double eastM = on.EastM + left[0] * acrossAt[which];
      const double northM = on.NorthM + left[1] * acrossAt[which];
      Put(out.PositionM, eastM - surface.NormalM[0] * section.ThicknessM,
          surface.HeightM - surface.NormalM[1] * section.ThicknessM,
          -(northM - surface.NormalM[2] * section.ThicknessM));
      Put(out.NormalM, -surface.NormalM[0], -surface.NormalM[1], surface.NormalM[2]);
      out.AcrossM.push_back((float)acrossAt[which]);
    }
  }

  const uint32_t perStation = (uint32_t)(kRibbonAcross * 2);
  for (size_t station = 0; station + 1 < stations; ++station) {
    const uint32_t here = (uint32_t)station * perStation;
    const uint32_t next = here + perStation;
    for (uint32_t which = 0; which + 1 < (uint32_t)kRibbonAcross; ++which) {
      out.Index.insert(out.Index.end(),
                       {here + which, next + which, here + which + 1,
                        here + which + 1, next + which, next + which + 1});
      const uint32_t under = (uint32_t)kRibbonAcross;
      out.Index.insert(out.Index.end(),
                       {here + under + which, here + under + which + 1, next + under + which,
                        here + under + which + 1, next + under + which + 1, next + under + which});
    }
    const uint32_t under = (uint32_t)kRibbonAcross;
    const uint32_t edge = (uint32_t)kRibbonAcross - 1;
    out.Index.insert(out.Index.end(),
                     {here, here + under, next, next, here + under, next + under});
    out.Index.insert(out.Index.end(), {here + edge, next + edge, here + under + edge,
                                       next + edge, next + under + edge, here + under + edge});
  }

  for (size_t station = 0; station + 1 < stations; ++station) {
    const double atM = fromM + (double)station * stepM;
    Placed on;
    if (!along.At(atM, on)) { continue; }
    out.TopAreaM2 += stepM * (acrossAt[kRibbonAcross - 1] - acrossAt[0]);
  }

  out.Stations = stations;
  out.Vertices = out.PositionM.size() / 3;
  out.Triangles = out.Index.size() / 3;
  out.Woven = true;
  return out;
}

} // namespace outshine

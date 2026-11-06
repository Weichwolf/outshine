#include <algorithm>
#include <expected>
#include <span>
#include <string_view>
#include "math/RenderFrame.h"
#include "RibbonSampling.h"
#include "Ribbon.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <vector>
#include <string>
#include <cstdint>

namespace outshine {

namespace {

constexpr size_t kVerticesPerStation = kRibbonAcross * 2 + 4;

void Put(std::vector<float> &into, double a, double b, double c) {
  into.push_back(static_cast<float>(a));
  into.push_back(static_cast<float>(b));
  into.push_back(static_cast<float>(c));
}

void ConnectStations(size_t stations,
                     std::span<const uint32_t> bands,
                     RibbonForm form,
                     Ribbon &out) {
  const auto perStation =
      static_cast<uint32_t>(form == RibbonForm::Surface ? kRibbonAcross : kVerticesPerStation);
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
      if (form == RibbonForm::ClosedShell) {
        const auto under = static_cast<uint32_t>(kRibbonAcross);
        out.Index.insert(out.Index.end(),
                         {here + under + which,
                          here + under + which + 1,
                          next + under + which,
                          here + under + which + 1,
                          next + under + which + 1,
                          next + under + which});
      }
    }
    if (form == RibbonForm::Surface) { continue; }
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

bool AppendSections(const ReferenceLine &along,
                    const Section &section,
                    std::span<const double> sampling,
                    RibbonForm form,
                    Ribbon &out) {
  for (const double atM : sampling) {
    Placed on;
    if (!along.At(atM, on)) {
      out.Error = "the reference line places nothing at " + std::to_string(atM) + " m";
      return false;
    }
    const auto samples = SampleRibbonSection(on, section, out.OriginM, form);
    for (size_t at = 0; at < samples.Count; ++at) {
      const auto &vertex = samples.Vertices[at];
      Put(out.PositionM, vertex.PositionM[0], vertex.PositionM[1], vertex.PositionM[2]);
      Put(out.NormalM, vertex.NormalM[0], vertex.NormalM[1], vertex.NormalM[2]);
      out.AcrossM.push_back(vertex.AcrossM);
    }
  }
  return true;
}

}

Ribbon Sweep(const ReferenceLine &along,
             const Section &section,
             double fromM,
             double toM,
             double stepM,
             RibbonForm form,
             std::span<const double> requiredStations) {
  Ribbon out;
  out.FromM = fromM;
  out.ToM = toM;

  const auto sampling = PrepareRibbonSampling(along, section, fromM, toM, stepM, requiredStations);
  if (!sampling) {
    out.Error = sampling.error();
    return out;
  }
  out.OriginM = sampling->OriginM;
  const size_t stations = sampling->StationsM.size();

  const size_t vertices = form == RibbonForm::Surface
                              ? stations * kRibbonAcross
                              : stations * kVerticesPerStation + 4 * kRibbonAcross;
  out.PositionM.reserve(vertices * 3);
  out.NormalM.reserve(vertices * 3);
  out.AcrossM.reserve(vertices);
  out.Index.reserve(form == RibbonForm::Surface
                        ? (stations - 1) * (kRibbonAcross - 1) * 6
                        : (stations - 1) * kRibbonAcross * 12 + (kRibbonAcross - 1) * 12);

  if (!AppendSections(along, section, sampling->StationsM, form, out)) { return out; }

  constexpr std::array<uint32_t, 3> allBands = {0, 1, 2};
  const std::span<const uint32_t> bands = allBands;
  const auto activeBands = section.ShoulderM == 0.0 ? bands.subspan(1, 1) : bands;
  ConnectStations(stations, activeBands, form, out);
  if (form == RibbonForm::ClosedShell) {
    CloseEnds(along, stations, activeBands, out);
    BoundaryNormals(stations, out);
  }

  out.Stations = stations;
  out.Vertices = out.PositionM.size() / 3;
  out.Triangles = out.Index.size() / 3;
  out.Woven = true;
  return out;
}

}

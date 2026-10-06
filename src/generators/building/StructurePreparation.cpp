#include "StructurePreparation.h"
#include "PreparedStructurePlan.h"
#include "BuildingScratch.h"
#include "Geodesy.h"
#include "math/Units.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint32_t kKnuthWord = 2654435761u;
constexpr uint32_t kSecondKnuthWord = 2246822519u;

[[nodiscard]] bool WasStopped(const std::atomic_bool *stopping) {
  return stopping != nullptr && stopping->load(std::memory_order_relaxed);
}

constexpr uint32_t kPlaceMixWord = 3266489917u;
constexpr double kMicroDegree = 1.0e6;
constexpr double kNoNearestYet = 1.0e29;
constexpr double kSameHeightM = 0.01;
constexpr double kNoLeastYet = 1.0e9;
constexpr double kOnStreetAcrossM = 14.0;
constexpr double kOffStreetAcrossM = 26.0;
constexpr double kFillHeightM = 5.0;
constexpr double kStoreyM = 2.9;
constexpr double kRoofAllowanceM = 3.2;
constexpr double kOnTheStreetM = 16.0;
constexpr double kCarriagewayM = 4.0;
constexpr int kInteriorGrid = 4;
constexpr double kInteriorSpanM = 20.0;
constexpr double kPadReachM = 60.0;
constexpr double kMostRingPoints = 512;

struct Ring {
  uint32_t First = 0;
  uint32_t Count = 0;
};

[[nodiscard]] double LatOf(std::span<const double> pts, size_t at) {
  return pts[at * 2];
}

[[nodiscard]] double LonOf(std::span<const double> pts, size_t at) {
  return pts[at * 2 + 1];
}

uint32_t PlaceHash(LongitudeLatitude at) {
  uint32_t h =
      static_cast<uint32_t>(static_cast<int32_t>(std::llround(at.LatitudeDeg * kMicroDegree))) *
      kKnuthWord;
  h ^= static_cast<uint32_t>(static_cast<int32_t>(std::llround(at.LongitudeDeg * kMicroDegree))) *
       kSecondKnuthWord;
  h ^= h >> 13u;
  h *= kPlaceMixWord;
  return h ^ (h >> 16u);
}

struct Plot {
  double AreaM2 = 0.0;
  double AcrossM = 0.0;
  double StandBackM = 0.0;
};

int DefaultStoreys(Plot of, LongitudeLatitude at) {
  const uint32_t h = PlaceHash(at);
  const bool onStreet = of.StandBackM >= 0.0 && of.StandBackM <= kOnTheStreetM;
  const bool aPlot = of.AreaM2 >= 70.0;
  int least = 1;
  int most = 2;
  if (onStreet && aPlot && of.AcrossM <= kOnStreetAcrossM) {
    least = 3;
    most = 5;
  } else if (onStreet && aPlot) {
    least = 2;
    most = 4;
  } else if (!aPlot || of.AcrossM > kOffStreetAcrossM) {
    least = 1;
    most = 2;
  } else {
    least = 1;
    most = 3;
  }
  return least + static_cast<int>(h % static_cast<uint32_t>(most - least + 1));
}

double RingAreaM2(std::span<const double> pts, Ring ring) {
  const LongitudeLatitudeHeight from{.LongitudeDeg = LonOf(pts, ring.First),
                                     .LatitudeDeg = LatOf(pts, ring.First)};
  double a = 0.0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const uint32_t j = (k + 1) % ring.Count;
    const EastNorth at = EnuOffsetM(
        from,
        {.LongitudeDeg = LonOf(pts, ring.First + k), .LatitudeDeg = LatOf(pts, ring.First + k)});
    const EastNorth next = EnuOffsetM(
        from,
        {.LongitudeDeg = LonOf(pts, ring.First + j), .LatitudeDeg = LatOf(pts, ring.First + j)});
    a += at.EastM * next.NorthM - next.EastM * at.NorthM;
  }
  return std::fabs(0.5 * a);
}

double AcrossM(std::span<const double> pts, Ring ring) {
  const LongitudeLatitudeHeight from{.LongitudeDeg = LonOf(pts, ring.First),
                                     .LatitudeDeg = LatOf(pts, ring.First)};
  double e0 = kBeyondAnyCoordinate;
  double e1 = -kBeyondAnyCoordinate;
  double n0 = kBeyondAnyCoordinate;
  double n1 = -kBeyondAnyCoordinate;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const EastNorth at = EnuOffsetM(
        from,
        {.LongitudeDeg = LonOf(pts, ring.First + k), .LatitudeDeg = LatOf(pts, ring.First + k)});
    e0 = std::min(e0, at.EastM);
    e1 = std::max(e1, at.EastM);
    n0 = std::min(n0, at.NorthM);
    n1 = std::max(n1, at.NorthM);
  }
  return std::min(e1 - e0, n1 - n0);
}

BuildingFrontage NearestStreet(std::span<const double> pts,
                               Ring ring,
                               std::span<const WayLine> ways,
                               double *standBackM,
                               const std::atomic_bool *stopping) {
  BuildingFrontage out;
  *standBackM = -1.0;
  const double refLat = LatOf(pts, ring.First);
  const double refLon = LonOf(pts, ring.First);
  double cE = 0.0;
  double cN = 0.0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const EastNorth at = EnuOffsetM(
        {.LongitudeDeg = refLon, .LatitudeDeg = refLat},
        {.LongitudeDeg = LonOf(pts, ring.First + k), .LatitudeDeg = LatOf(pts, ring.First + k)});
    cE += at.EastM;
    cN += at.NorthM;
  }
  cE /= static_cast<double>(ring.Count);
  cN /= static_cast<double>(ring.Count);
  const double padDeg = (kOnTheStreetM + kPadReachM) / kMPerDegLat;
  double best = kBeyondAnyCoordinate;
  double bE = 0.0;
  double bN = 0.0;
  double bDirE = 0.0;
  double bDirN = 0.0;
  double bHalf = 0.0;
  for (const WayLine &w : ways) {
    if (WasStopped(stopping)) { return out; }
    if (w.HalfWidthM * 2.0 < kCarriagewayM) { continue; }
    if (refLat < w.MinLat - padDeg || refLat > w.MaxLat + padDeg) { continue; }
    if (refLon < w.MinLon - padDeg || refLon > w.MaxLon + padDeg) { continue; }
    for (size_t k = 0; k + 3 < w.LatLon.size(); k += 2) {
      if (WasStopped(stopping)) { return out; }
      const LongitudeLatitudeHeight from{.LongitudeDeg = refLon, .LatitudeDeg = refLat};
      const EastNorth a =
          EnuOffsetM(from, {.LongitudeDeg = w.LatLon[k + 1], .LatitudeDeg = w.LatLon[k]});
      const EastNorth b =
          EnuOffsetM(from, {.LongitudeDeg = w.LatLon[k + 3], .LatitudeDeg = w.LatLon[k + 2]});
      const double aE = a.EastM;
      const double aN = a.NorthM;
      const double dE = b.EastM - aE;
      const double dN = b.NorthM - aN;
      const double len2 = dE * dE + dN * dN;
      if (len2 < kLeastRunM) { continue; }
      double t = ((cE - aE) * dE + (cN - aN) * dN) / len2;
      t = std::clamp(t, 0.0, 1.0);
      const double pE = aE + dE * t;
      const double pN = aN + dN * t;
      const double d = std::hypot(cE - pE, cN - pN);
      if (d >= best) { continue; }
      best = d;
      bE = pE;
      bN = pN;
      const double len = std::sqrt(len2);
      bDirE = dE / len;
      bDirN = dN / len;
      bHalf = w.HalfWidthM;
    }
  }
  if (best > kNoNearestYet || best <= bHalf) { return out; }
  const double toE = (bE - cE) / best;
  const double toN = (bN - cN) / best;
  out.Known = true;
  out.KerbEm = bE - toE * bHalf;
  out.KerbNm = bN - toN * bHalf;
  out.AlongE = bDirE;
  out.AlongN = bDirN;
  out.ToStreetE = toE;
  out.ToStreetN = toN;
  *standBackM = best - bHalf;
  return out;
}

bool InsideRing(std::span<const double> pts, Ring ring, double lat, double lon) {
  bool in = false;
  for (uint32_t k = 0, j = ring.Count - 1; k < ring.Count; j = k++) {
    const double kLat = LatOf(pts, ring.First + k);
    const double kLon = LonOf(pts, ring.First + k);
    const double jLat = LatOf(pts, ring.First + j);
    const double jLon = LonOf(pts, ring.First + j);
    if ((kLat > lat) == (jLat > lat)) { continue; }
    if (lon < (jLon - kLon) * (lat - kLat) / (jLat - kLat) + kLon) { in = !in; }
  }
  return in;
}

struct Seated {
  double BaseM = 0.0;
  double SeatM = 0.0;
  bool Stood = false;
};

Seated RingBase(const outshine::Ground::HeightField &heights,
                std::span<const double> pts,
                Ring ring,
                std::vector<double> &corners) {
  corners.clear();
  const auto sampled = [&heights](double lat, double lon) {
    return heights.At({.LongitudeDeg = lon, .LatitudeDeg = lat}).AslM();
  };
  bool stood = true;
  const auto at = [&sampled, &stood](double lat, double lon) {
    const std::optional<double> held = sampled(lat, lon);
    stood = stood && held.has_value();
    return held.value_or(0.0);
  };
  double lowest = kNoLeastYet;
  double highest = -kNoLeastYet;
  double summed = 0.0;
  size_t took = 0;
  double southest = kNoLeastYet;
  double northest = -kNoLeastYet;
  double westest = kNoLeastYet;
  double eastest = -kNoLeastYet;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const double lat = LatOf(pts, ring.First + k);
    const double lon = LonOf(pts, ring.First + k);
    const double aslM = at(lat, lon);
    corners.push_back(aslM);
    lowest = std::min(lowest, aslM);
    highest = std::max(highest, aslM);
    summed += aslM;
    ++took;
    southest = std::min(southest, lat);
    northest = std::max(northest, lat);
    westest = std::min(westest, lon);
    eastest = std::max(eastest, lon);
  }
  const double tall = (northest - southest) * kMPerDegLat;
  const double wide =
      (eastest - westest) * kMPerDegLon * std::cos(0.5 * (northest + southest) * kDeg2Rad);
  if (std::max(tall, wide) >= kInteriorSpanM) {
    for (int row = 1; row < kInteriorGrid; ++row) {
      for (int column = 1; column < kInteriorGrid; ++column) {
        const double lat = southest + (northest - southest) * static_cast<double>(row) /
                                          static_cast<double>(kInteriorGrid);
        const double lon = westest + (eastest - westest) * static_cast<double>(column) /
                                         static_cast<double>(kInteriorGrid);
        if (!InsideRing(pts, ring, lat, lon)) { continue; }
        const std::optional<double> inside = sampled(lat, lon);
        if (!inside) { continue; }
        const double aslM = *inside;
        lowest = std::min(lowest, aslM);
        highest = std::max(highest, aslM);
        summed += aslM;
        ++took;
      }
    }
  }
  return {.BaseM = lowest,
          .SeatM = took > 0 ? summed / static_cast<double>(took) : highest,
          .Stood = stood};
}

struct Spread {
  double LowLat = 0, HighLat = 0, LowLon = 0, HighLon = 0;
};

bool HasSourceObject(const RawTile &raw, Data::SourceObjectId id) {
  return raw.SourceInputs.Objects ? raw.SourceInputs.Objects->Contains(id) : id.Id == 0;
}

bool ValidStructureInput(const RawTile::Structure &structure, const RawTile &raw) {
  const size_t holes = raw.Holes.size();
  return HasSourceObject(raw, structure.SourceId) && structure.FirstHole <= holes &&
         structure.HoleCount <= holes - structure.FirstHole &&
         (!structure.HeightOrigin ||
          (std::isfinite(structure.HeightM) && structure.HeightM > structure.MinimumHeightM)) &&
         std::isfinite(structure.MinimumHeightM) &&
         (structure.MinimumHeightM == 0.0 ||
          (std::isfinite(structure.HeightM) && structure.HeightM > structure.MinimumHeightM));
}

bool HasSourceHeight(const RawTile::Structure &structure) {
  if (structure.HeightOrigin || structure.MinimumHeightM != 0.0) { return true; }
  return structure.HeightM > 0.0 && std::fabs(structure.HeightM - kFillHeightM) > kSameHeightM;
}

::outshine::Ground::BuildingHeightSource
HeightSourceOf(const RawTile::Structure &structure) noexcept {
  if (structure.HeightOrigin &&
      *structure.HeightOrigin != outshine::Ground::BuildingHeightOrigin::Declared) {
    return ::outshine::Ground::BuildingHeightSource::Generated;
  }
  return ::outshine::Ground::BuildingHeightSource::Declared;
}

Spread RingBounds(std::span<const double> pts, Ring ring) {
  Spread bounds{.LowLat = kNoLeastYet,
                .HighLat = -kNoLeastYet,
                .LowLon = kNoLeastYet,
                .HighLon = -kNoLeastYet};
  for (uint32_t k = 0; k < ring.Count; k++) {
    bounds.LowLat = std::min(bounds.LowLat, LatOf(pts, ring.First + k));
    bounds.HighLat = std::max(bounds.HighLat, LatOf(pts, ring.First + k));
    bounds.LowLon = std::min(bounds.LowLon, LonOf(pts, ring.First + k));
    bounds.HighLon = std::max(bounds.HighLon, LonOf(pts, ring.First + k));
  }
  return bounds;
}
}

std::vector<WayLine> StructureWays(const RawTile &raw) {
  const std::span<const double> pts = raw.LatLon;
  std::vector<WayLine> ways;
  ways.reserve(raw.Ways.size());
  for (const RawTile::Way &w : raw.Ways) {
    WayLine line;
    line.LatLon = std::span<const double>(pts.data() + static_cast<size_t>(w.LocalFirst) * 2,
                                          static_cast<size_t>(w.PointCount) * 2);
    line.HalfWidthM = w.HalfWidthM;
    line.MinLat = kBeyondAnyCoordinate;
    line.MinLon = kBeyondAnyCoordinate;
    line.MaxLat = -kBeyondAnyCoordinate;
    line.MaxLon = -kBeyondAnyCoordinate;
    for (uint32_t k = 0; k < w.PointCount; ++k) {
      line.MinLat = std::min(line.MinLat, LatOf(pts, w.LocalFirst + k));
      line.MaxLat = std::max(line.MaxLat, LatOf(pts, w.LocalFirst + k));
      line.MinLon = std::min(line.MinLon, LonOf(pts, w.LocalFirst + k));
      line.MaxLon = std::max(line.MaxLon, LonOf(pts, w.LocalFirst + k));
    }
    ways.push_back(line);
  }
  return ways;
}

std::expected<std::optional<PreparedStructure>, StructureBakeError>
EnrichStructure(const RawTile &raw,
                const Ground::HeightField &heights,
                const RawTile::Structure &one,
                std::span<const double> points,
                std::span<const WayLine> ways,
                std::vector<double> &corners,
                BakedTile &diagnostics,
                const std::atomic_bool *stopping) {
  if (one.Cell.Index == 0 || one.Cell.Index > kStructureCellsPerTile) {
    return std::unexpected(StructureBakeErrorKind::InvalidCell);
  }
  const Ring ring{.First = one.LocalFirst, .Count = one.PointCount};
  if (ring.Count < 3 || ring.Count > kMostRingPoints) {
    ++diagnostics.SkippedRings;
    return std::optional<PreparedStructure>{};
  }
  if (ring.First > points.size() / 2 || ring.Count > points.size() / 2 - ring.First) {
    return std::unexpected(StructureMeshError::InvalidPlan);
  }
  const Seated seated = RingBase(heights, points, ring, corners);
  if (!seated.Stood) {
    ++diagnostics.NoGround;
    return std::optional<PreparedStructure>{};
  }
  const double base = seated.BaseM;
  const double seat = seated.SeatM;

  const Spread bounds = RingBounds(points, ring);
  const double perLonM = kMPerDegLon * std::cos(0.5 * (bounds.LowLat + bounds.HighLat) * kDeg2Rad);
  const double acrossM = std::max((bounds.HighLat - bounds.LowLat) * kMPerDegLat,
                                  (bounds.HighLon - bounds.LowLon) * perLonM);

  double standBackM = -1.0;
  const BuildingFrontage street = NearestStreet(points, ring, ways, &standBackM, stopping);
  if (WasStopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }

  if (!ValidStructureInput(one, raw)) { return std::unexpected(StructureMeshError::InvalidPlan); }
  ::outshine::Ground::BuildingFootprint fp{};
  fp.MinimumHeightM = static_cast<float>(one.MinimumHeightM);
  fp.FirstPoint = one.SourceFirst;
  fp.FirstHole = one.SourceFirstHole;
  fp.HoleCount = one.HoleCount;
  fp.PointCount = ring.Count;
  fp.Street = street;
  if (HasSourceHeight(one)) {
    fp.HeightM = static_cast<float>(one.HeightM);
    fp.Source = HeightSourceOf(one);
  } else {
    const int storeys = DefaultStoreys(
        {.AreaM2 = RingAreaM2(points, ring),
         .AcrossM = AcrossM(points, ring),
         .StandBackM = standBackM},
        {.LongitudeDeg = LonOf(points, ring.First), .LatitudeDeg = LatOf(points, ring.First)});
    fp.HeightM = static_cast<float>(static_cast<double>(storeys) * kStoreyM + kRoofAllowanceM);
    fp.Source = ::outshine::Ground::BuildingHeightSource::Generated;
  }
  fp.BaseM = static_cast<float>(base);
  fp.FootM = static_cast<float>(base);
  fp.SeatM = static_cast<float>(seat);
  return PreparedStructure{.Layout = one,
                           .Standing = fp,
                           .Bounds = {.MinLonDeg = bounds.LowLon,
                                      .MinLatDeg = bounds.LowLat,
                                      .MaxLonDeg = bounds.HighLon,
                                      .MaxLatDeg = bounds.HighLat},
                           .BaseAslM = base,
                           .SeatAslM = seat,
                           .AreaM2 = RingAreaM2(points, ring),
                           .AcrossM = acrossM};
}

std::expected<PreparedStructureTile, StructureBakeError> PrepareStructureTile(
    const RawTile &raw, const Ground::HeightField &heights, const std::atomic_bool *stopping) {
  if (WasStopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
  PreparedStructureTile base;
  base.PointsLatLon = raw.LatLon;
  base.Holes = raw.Holes;
  base.Origin = raw.SourceInputs.Origin;
  base.AnchorEcef = raw.AnchorEcef;
  base.TileSpanM = raw.TileSpanM;
  base.Extent = raw.Extent;
  base.FallbackHeights = heights.Fallback();
  base.Structures.reserve(raw.Structures.size());
  base.Surfaces.reserve(raw.Structures.size());
  BuildingScratch scratch;
  const auto ways = StructureWays(raw);
  std::vector<double> corners;
  BakedTile diagnostics;
  for (const auto &one : raw.Structures) {
    if (WasStopped(stopping)) { return std::unexpected(StructureBakeErrorKind::Cancelled); }
    auto prepared =
        EnrichStructure(raw, heights, one, raw.LatLon, ways, corners, diagnostics, stopping);
    if (!prepared) { return std::unexpected(prepared.error()); }
    if (!*prepared) { continue; }
    (**prepared).CornerFirst = base.CornerAslM.size();
    base.CornerAslM.insert(base.CornerAslM.end(), corners.begin(), corners.end());
    const auto plan =
        PreparedStructurePlan(**prepared, base.PointsLatLon, base.Holes, corners, base.AnchorEcef);
    auto surface = BuildingSurface::Prepare(plan, scratch);
    if (!surface) { return std::unexpected(surface.error()); }
    base.Structures.push_back(**prepared);
    base.Surfaces.push_back(std::move(*surface));
  }
  base.SkippedRings = diagnostics.SkippedRings;
  base.NoGround = diagnostics.NoGround;
  return base;
}
}

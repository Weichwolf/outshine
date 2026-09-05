#include "StructureBake.h"
#include <bit>
#include "Digest.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <vector>

#include "math/Units.h"
#include "Geodesy.h"
#include <generate/Generate.h>

namespace outshine::Generators {

namespace {

using outshine::Ground::BuildingField;

constexpr double kBlocksPerTile = 8.0;
constexpr int64_t kCellBiasTiles = 0x20000000LL;
constexpr uint32_t kKnuthWord = 2654435761u;
constexpr uint32_t kSecondKnuthWord = 2246822519u;
constexpr uint32_t kPlaceMixWord = 3266489917u;
constexpr double kMicroDegree = 1.0e6;
constexpr double kNoNearestYet = 1.0e29;
constexpr double kSameHeightM = 0.01;
constexpr double kNoLeastYet = 1.0e9;
constexpr double kOnStreetAcrossM = 14.0;
constexpr double kOffStreetAcrossM = 26.0;
constexpr double kFillHeightM = 5.0;
constexpr double kStoreyM = 2.9;
constexpr double kNearestSeenM = 0.5 * kStoreyM;
constexpr double kArchitectureM = 0.33;
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

Frontage NearestStreet(std::span<const double> pts,
                       Ring ring,
                       std::span<const WayLine> ways,
                       double *standBackM) {
  Frontage out;
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
    if (w.HalfWidthM * 2.0 < kCarriagewayM) { continue; }
    if (refLat < w.MinLat - padDeg || refLat > w.MaxLat + padDeg) { continue; }
    if (refLon < w.MinLon - padDeg || refLon > w.MaxLon + padDeg) { continue; }
    for (size_t k = 0; k + 3 < w.LatLon.size(); k += 2) {
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

struct Lumped {
  double LowLat = 0.0, HighLat = 0.0, LowLon = 0.0, HighLon = 0.0;
  double BaseSum = 0.0, SeatSum = 0.0, HeightSum = 0.0;
  int Count = 0;
  double PitchedAreaM2 = 0.0, RoofAreaM2 = 0.0;
  Detail Level = Detail::Fine;
};

struct Spread {
  double LowLat = 0.0, HighLat = 0.0, LowLon = 0.0, HighLon = 0.0;
};

struct Standing {
  double BaseM = 0.0, SeatM = 0.0, HeightM = 0.0;
  double RoofAreaM2 = 0.0;
  bool Pitched = false;
  Detail Level = Detail::Fine;
};

void Lump(std::map<uint64_t, Lumped> &into, Spread over, Standing at, double cellM) {
  const auto cellLat =
      static_cast<int64_t>(std::floor(0.5 * (over.LowLat + over.HighLat) * kMPerDegLat / cellM));
  const auto cellLon =
      static_cast<int64_t>(std::floor(0.5 * (over.LowLon + over.HighLon) * kMPerDegLon / cellM));
  Lumped &block = into[(static_cast<uint64_t>(cellLat + kCellBiasTiles) << 32U) |
                       static_cast<uint64_t>(cellLon + kCellBiasTiles)];
  if (block.Count == 0) {
    block.LowLat = over.LowLat;
    block.HighLat = over.HighLat;
    block.LowLon = over.LowLon;
    block.HighLon = over.HighLon;
  } else {
    block.LowLat = std::min(block.LowLat, over.LowLat);
    block.HighLat = std::max(block.HighLat, over.HighLat);
    block.LowLon = std::min(block.LowLon, over.LowLon);
    block.HighLon = std::max(block.HighLon, over.HighLon);
  }
  block.BaseSum += at.BaseM;
  block.SeatSum += at.SeatM;
  block.HeightSum += at.HeightM;
  block.RoofAreaM2 += at.RoofAreaM2;
  if (at.Pitched) { block.PitchedAreaM2 += at.RoofAreaM2; }
  block.Level = std::max(block.Level, at.Level);
  ++block.Count;
}

void RaiseLump(const Lumped &of,
               const RawTile &raw,
               const StructureMesher &mesher,
               MeshScratch &scratch,
               std::vector<double> &corners,
               Raised &into) {
  if (of.Count == 0) { return; }
  const auto over = static_cast<double>(of.Count);
  const double midLat = 0.5 * (of.LowLat + of.HighLat);
  const double midLon = 0.5 * (of.LowLon + of.HighLon);
  const double spanLatM = (of.HighLat - of.LowLat) * kMPerDegLat;
  const double spanLonM = (of.HighLon - of.LowLon) * kMPerDegLon * std::cos(midLat * kDeg2Rad);
  const double boxM2 = spanLatM * spanLonM;
  const double shrink = boxM2 > 0.0 && of.RoofAreaM2 > 0.0 && of.RoofAreaM2 < boxM2
                            ? std::sqrt(of.RoofAreaM2 / boxM2)
                            : 1.0;
  const double halfLat = 0.5 * (of.HighLat - of.LowLat) * shrink;
  const double halfLon = 0.5 * (of.HighLon - of.LowLon) * shrink;
  const double lowLat = midLat - halfLat;
  const double highLat = midLat + halfLat;
  const double lowLon = midLon - halfLon;
  const double highLon = midLon + halfLon;
  const std::array<double, 8> ring = {
      lowLat, lowLon, lowLat, highLon, highLat, highLon, highLat, lowLon};
  corners.assign(4, of.BaseSum / over);
  StructurePlan plan;
  plan.RingLatLon = std::span<const double>(ring.data(), ring.size());
  plan.BaseAslM = of.BaseSum / over;
  plan.SeatAslM = of.SeatSum / over;
  plan.FootAslM = of.BaseSum / over;
  plan.CornerAslM = std::span<const double>(corners.data(), corners.size());
  plan.HeightM = of.HeightSum / over;
  plan.HeightMeasured = false;
  plan.Street = {};
  plan.AnchorEcef = raw.AnchorEcef;
  plan.FocalPx = raw.FocalPx;
  plan.Coarseness = of.Level;
  plan.PitchedShare = of.RoofAreaM2 > 0.0 ? of.PitchedAreaM2 / of.RoofAreaM2 : kPitchedShareUnknown;
  (void)mesher.Mesh(plan, scratch, into);
}

[[nodiscard]] uint64_t DigestOver(const Raised &built) {
  uint64_t mixed = kDigestBasis;
  const auto fold = [&mixed](uint64_t one) { mixed = (mixed ^ one) * kDigestPrime; };
  const auto foldVertex = [&fold](const StoredVertex &one) {
    const auto *const held = reinterpret_cast<const float *>(&one);
    for (size_t at = 0; at < kStoredVertexFloats; ++at) { fold(std::bit_cast<uint32_t>(held[at])); }
  };
  for (const StoredVertex &one : built.WallCorners) { foldVertex(one); }
  for (const StoredVertex &one : built.RoofCorners) { foldVertex(one); }
  for (const uint32_t one : built.WallRun) { fold(one); }
  for (const uint32_t one : built.RoofRun) { fold(one); }
  return mixed;
}

[[nodiscard]] Cooked CookedOver(const RawTile &raw,
                                std::span<const StoredVertex> corners,
                                std::span<const uint32_t> run) {
  if (run.size() < 3 || raw.ClusterTriangles == 0) { return {}; }
  const std::span<const float> positions(reinterpret_cast<const float *>(corners.data()),
                                         corners.size() * kStoredVertexFloats);
  return CookClusters(positions, run, raw.ClusterTriangles, static_cast<int>(kStoredVertexFloats));
}

std::vector<WayLine> LinesOf(const RawTile &raw) {
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

} // namespace

void BakeStructures(const RawTile &raw,
                    const outshine::Ground::HeightField &heights,
                    const StructureMesher &mesher,
                    MeshScratch &scratch,
                    BakedTile &out) {
  out.Built.Clear();
  out.Prints.clear();
  out.SeatSpreadM.clear();
  out.AcrossM.clear();
  out.OsmHeights = 0;
  out.DefaultHeights = 0;
  out.Fronted = 0;
  out.Lumped = 0;
  out.Blocks = 0;
  out.NoGround = 0;

  const std::span<const double> pts = raw.LatLon;
  const std::vector<WayLine> ways = LinesOf(raw);

  std::map<uint64_t, Lumped> lumps;
  std::vector<double> corners;
  const double awayAtLeastM = std::max(raw.AwayM, kNearestSeenM);
  const double statedM =
      raw.TileSpanM > 0.0 && raw.Extent > 0 ? raw.TileSpanM / static_cast<double>(raw.Extent) : 0.0;

  for (const RawTile::Structure &one : raw.Structures) {
    const Ring ring{.First = one.LocalFirst, .Count = one.PointCount};
    if (ring.Count < 3 || ring.Count > kMostRingPoints) { continue; }
    const Seated seated = RingBase(heights, pts, ring, corners);
    if (!seated.Stood) {
      ++out.NoGround;
      continue;
    }
    const double base = seated.BaseM;
    const double seat = seated.SeatM;

    double lowLat = kNoLeastYet;
    double highLat = -kNoLeastYet;
    double lowLon = kNoLeastYet;
    double highLon = -kNoLeastYet;
    for (uint32_t k = 0; k < ring.Count; k++) {
      lowLat = std::min(lowLat, LatOf(pts, ring.First + k));
      highLat = std::max(highLat, LatOf(pts, ring.First + k));
      lowLon = std::min(lowLon, LonOf(pts, ring.First + k));
      highLon = std::max(highLon, LonOf(pts, ring.First + k));
    }
    const double perLonM = kMPerDegLon * std::cos(0.5 * (lowLat + highLat) * kDeg2Rad);
    out.SeatSpreadM.push_back(seat - base);
    out.AcrossM.push_back(std::max((highLat - lowLat) * kMPerDegLat, (highLon - lowLon) * perLonM));

    double standBackM = -1.0;
    const Frontage street = NearestStreet(pts, ring, ways, &standBackM);

    BuildingField::Footprint fp{};
    fp.FirstPoint = one.SourceFirst;
    fp.PointCount = ring.Count;
    fp.Street = street;
    if (street.Known) { out.Fronted++; }
    if (one.HeightM > 0.0 && std::fabs(one.HeightM - kFillHeightM) > kSameHeightM) {
      fp.HeightM = static_cast<float>(one.HeightM);
      fp.Source = BuildingField::HeightSource::Osm;
      out.OsmHeights++;
    } else {
      const int storeys = DefaultStoreys(
          {.AreaM2 = RingAreaM2(pts, ring),
           .AcrossM = AcrossM(pts, ring),
           .StandBackM = standBackM},
          {.LongitudeDeg = LonOf(pts, ring.First), .LatitudeDeg = LatOf(pts, ring.First)});
      fp.HeightM = static_cast<float>(static_cast<double>(storeys) * kStoreyM + kRoofAllowanceM);
      fp.Source = BuildingField::HeightSource::Default;
      out.DefaultHeights++;
    }
    fp.BaseM = static_cast<float>(base);
    fp.FootM = static_cast<float>(base);
    fp.SeatM = static_cast<float>(seat);

    Detail level = Detail::Fine;
    if (Unseen(std::max(kArchitectureM, statedM), raw.FocalPx, awayAtLeastM)) {
      level = Detail::Shell;
    }
    if (level == Detail::Shell &&
        Unseen(0.5 * raw.TileSpanM / kBlocksPerTile, raw.FocalPx, awayAtLeastM)) {
      level = Detail::Massed;
    }
    fp.Coarseness = level;
    out.Prints.push_back(fp);

    if (level >= Detail::Massed) {
      Lump(lumps,
           {.LowLat = lowLat, .HighLat = highLat, .LowLon = lowLon, .HighLon = highLon},
           {.BaseM = base,
            .SeatM = seat,
            .HeightM = fp.HeightM,
            .RoofAreaM2 = RingAreaM2(pts, ring),
            .Pitched = one.Pitched != 0,
            .Level = level},
           raw.TileSpanM / kBlocksPerTile);
      ++out.Lumped;
      continue;
    }

    StructurePlan plan;
    plan.RingLatLon = std::span<const double>(pts.data() + static_cast<size_t>(ring.First) * 2,
                                              static_cast<size_t>(ring.Count) * 2);
    plan.BaseAslM = fp.BaseM;
    plan.SeatAslM = fp.SeatM;
    plan.FootAslM = fp.FootM;
    plan.CornerAslM = std::span<const double>(corners.data(), corners.size());
    plan.HeightM = fp.HeightM;
    plan.HeightMeasured = fp.Source == BuildingField::HeightSource::Osm;
    plan.Street = fp.Street;
    plan.AnchorEcef = raw.AnchorEcef;
    plan.FocalPx = raw.FocalPx;
    plan.Coarseness = fp.Coarseness;
    (void)mesher.Mesh(plan, scratch, out.Built);
  }

  for (const auto &[where, block] : lumps) {
    (void)where;
    RaiseLump(block, raw, mesher, scratch, corners, out.Built);
  }
  out.Blocks = static_cast<int>(lumps.size());
  out.Walls = CookedOver(raw, out.Built.WallCorners, out.Built.WallRun);
  out.Roofs = CookedOver(raw, out.Built.RoofCorners, out.Built.RoofRun);
  out.Digest = DigestOver(out.Built);
}

} // namespace outshine::Generators

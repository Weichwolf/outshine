#include "math/Units.h"
#include "BuildingField.h"
#include "math/Vec3.h"

#include <chrono>

#include "Geodesy.h"
#include "Log.h"
#include "TerrainLoader.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <numbers>
#include <array>
#include <map>
#include <optional>
#include <span>
#include <utility>
#include <vector>
#include <ratio>

namespace outshine::Ground {

constexpr double kMassedAtPx = 8.0;

constexpr int64_t kCellBiasTiles = 0x20000000LL;

constexpr uint32_t kKnuthWord = 2654435761u;
constexpr uint32_t kSecondKnuthWord = 2246822519u;
constexpr double kNoNearestYet = 1.0e29;
constexpr double kSameHeightM = 0.01;

constexpr double kNoLeastYet = 1.0e9;

namespace {

constexpr uint32_t kPlaceMixWord = 3266489917u;
constexpr double kMicroDegree = 1.0e6;
constexpr double kOnStreetAcrossM = 14.0;
constexpr double kOffStreetAcrossM = 26.0;

constexpr double kFillHeightM = 5.0;

constexpr double kStoreyM = 2.9;
constexpr double kRoofAllowanceM = 3.2;

constexpr double kOnTheStreetM = 16.0;

constexpr double kCarriagewayM = 4.0;

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

double RingAreaM2(const OsmField &field, const OsmField::Ring &ring) {
  const std::span<const double> pts = field.Points();
  const double refLat = pts[static_cast<size_t>(ring.First) * 2];
  const double refLon = pts[static_cast<size_t>(ring.First) * 2 + 1];
  double a = 0.0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const uint32_t j = (k + 1) % ring.Count;
    const LongitudeLatitudeHeight from{.LongitudeDeg = refLon, .LatitudeDeg = refLat};
    const EastNorth at =
        EnuOffsetM(from,
                   {.LongitudeDeg = pts[(static_cast<size_t>(ring.First) + k) * 2 + 1],
                    .LatitudeDeg = pts[(static_cast<size_t>(ring.First) + k) * 2]});
    const EastNorth next =
        EnuOffsetM(from,
                   {.LongitudeDeg = pts[(static_cast<size_t>(ring.First) + j) * 2 + 1],
                    .LatitudeDeg = pts[(static_cast<size_t>(ring.First) + j) * 2]});
    a += at.EastM * next.NorthM - next.EastM * at.NorthM;
  }
  return std::fabs(0.5 * a);
}

double AcrossM(const OsmField &field, const OsmField::Ring &ring) {
  const std::span<const double> pts = field.Points();
  const double refLat = pts[static_cast<size_t>(ring.First) * 2];
  const double refLon = pts[static_cast<size_t>(ring.First) * 2 + 1];
  double e0 = kBeyondAnyCoordinate;
  double e1 = -kBeyondAnyCoordinate;
  double n0 = kBeyondAnyCoordinate;
  double n1 = -kBeyondAnyCoordinate;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const EastNorth at =
        EnuOffsetM({.LongitudeDeg = refLon, .LatitudeDeg = refLat},
                   {.LongitudeDeg = pts[(static_cast<size_t>(ring.First) + k) * 2 + 1],
                    .LatitudeDeg = pts[(static_cast<size_t>(ring.First) + k) * 2]});
    e0 = std::min(e0, at.EastM);
    e1 = std::max(e1, at.EastM);
    n0 = std::min(n0, at.NorthM);
    n1 = std::max(n1, at.NorthM);
  }
  return std::min(e1 - e0, n1 - n0);
}

Frontage NearestStreet(const OsmField &field,
                       const OsmField::Ring &ring,
                       std::span<const WayLine> ways,
                       double *standBackM) {
  Frontage out;
  *standBackM = -1.0;
  const std::span<const double> pts = field.Points();
  const double refLat = pts[static_cast<size_t>(ring.First) * 2];
  const double refLon = pts[static_cast<size_t>(ring.First) * 2 + 1];
  double cE = 0.0;
  double cN = 0.0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const EastNorth at =
        EnuOffsetM({.LongitudeDeg = refLon, .LatitudeDeg = refLat},
                   {.LongitudeDeg = pts[(static_cast<size_t>(ring.First) + k) * 2 + 1],
                    .LatitudeDeg = pts[(static_cast<size_t>(ring.First) + k) * 2]});
    cE += at.EastM;
    cN += at.NorthM;
  }
  cE /= static_cast<double>(ring.Count);
  cN /= static_cast<double>(ring.Count);

  const double padDeg = (kOnTheStreetM + 60.0) / 111000.0;
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

bool InsideRing(std::span<const double> pts, const OsmField::Ring &ring, double lat, double lon) {
  bool in = false;
  for (uint32_t k = 0, j = ring.Count - 1; k < ring.Count; j = k++) {
    const double kLat = pts[(static_cast<size_t>(ring.First) + k) * 2];
    const double kLon = pts[(static_cast<size_t>(ring.First) + k) * 2 + 1];
    const double jLat = pts[(static_cast<size_t>(ring.First) + j) * 2];
    const double jLon = pts[(static_cast<size_t>(ring.First) + j) * 2 + 1];
    if ((kLat > lat) == (jLat > lat)) { continue; }
    if (lon < (jLon - kLon) * (lat - kLat) / (jLat - kLat) + kLon) { in = !in; }
  }
  return in;
}

constexpr int kInteriorGrid = 4;
constexpr double kInteriorSpanM = 20.0;
constexpr double kMetresPerDegree = 111320.0;

} // namespace

GroundSample BuildingField::RingBase(const GroundQuery &ground,
                                     const OsmField &field,
                                     const OsmField::Ring &ring,
                                     std::vector<double> *corners,
                                     double *seatAslM) {
  const std::span<const double> pts = field.Points();
  if (corners != nullptr) { corners->clear(); }
  if (seatAslM != nullptr) { *seatAslM = 0.0; }
  if (ring.Count == 0) { return GroundSample::Missing(); }
  double lowest = kNoLeastYet;
  double highest = -kNoLeastYet;
  double summed = 0.0;
  size_t took = 0;
  double southest = kNoLeastYet;
  double northest = -kNoLeastYet;
  double westest = kNoLeastYet;
  double eastest = -kNoLeastYet;
  int coarsest = 0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const double lat = pts[(static_cast<size_t>(ring.First) + k) * 2];
    const double lon = pts[(static_cast<size_t>(ring.First) + k) * 2 + 1];
    const GroundSample g = ground.At({.LongitudeDeg = lon, .LatitudeDeg = lat});
    const std::optional<double> stood = g.AslM();
    if (!stood) { return g; }
    const double aslM = *stood;
    if (corners != nullptr) { corners->push_back(aslM); }
    lowest = std::min(lowest, aslM);
    highest = std::max(highest, aslM);
    summed += aslM;
    ++took;
    southest = std::min(southest, lat);
    northest = std::max(northest, lat);
    westest = std::min(westest, lon);
    eastest = std::max(eastest, lon);
    coarsest = std::max(coarsest, g.CoarseBy());
  }

  const double tall = (northest - southest) * kMetresPerDegree;
  const double wide =
      (eastest - westest) * kMetresPerDegree * std::cos(0.5 * (northest + southest) * kDeg2Rad);
  if (std::max(tall, wide) >= kInteriorSpanM) {
    for (int row = 1; row < kInteriorGrid; ++row) {
      for (int column = 1; column < kInteriorGrid; ++column) {
        const double lat = southest + (northest - southest) * static_cast<double>(row) /
                                          static_cast<double>(kInteriorGrid);
        const double lon = westest + (eastest - westest) * static_cast<double>(column) /
                                         static_cast<double>(kInteriorGrid);
        if (!InsideRing(pts, ring, lat, lon)) { continue; }
        const GroundSample g = ground.At({.LongitudeDeg = lon, .LatitudeDeg = lat});
        const std::optional<double> stood = g.AslM();
        if (!stood) { continue; }
        const double aslM = *stood;
        lowest = std::min(lowest, aslM);
        highest = std::max(highest, aslM);
        summed += aslM;
        ++took;
        coarsest = std::max(coarsest, g.CoarseBy());
      }
    }
  }
  if (seatAslM != nullptr) { *seatAslM = took > 0 ? summed / static_cast<double>(took) : highest; }
  return GroundSample::At(lowest).Coarser(coarsest);
}

bool BuildingField::TileGroundResolved(const GroundQuery &ground,
                                       const OsmField &field,
                                       FeatureRun over,
                                       int layer) {
  const std::span<const OsmField::Feature> feats = field.Features();
  for (size_t i = over.From; i < over.To; i++) {
    const OsmField::Feature &f = feats[i];
    if (f.Type != 3 || std::cmp_not_equal(f.Layer, layer)) { continue; }
    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) { continue; }
      const GroundSample base = RingBase(ground, field, ring, nullptr);
      if (base.Where() == GroundSample::State::Pending || base.CoarseBy() > 0) { return false; }
    }
  }
  return true;
}

void BuildingField::AnchorAt(const Vec3 &ecef) {
  assert(Prints_.empty());
  for (int c = 0; c < 3; c++) { Anchor_[c] = ecef[c]; }
  Anchored_ = true;
}

void BuildingField::Lump(std::map<uint64_t, Lumped> &into, Spread over, Standing at, double cellM) {
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
  ++block.Count;
}

double BuildingField::AwayFromCentreM(const OsmField &field, uint32_t tile) const {
  const std::span<const OsmField::Tile> tiles = field.Tiles();
  if (tile >= tiles.size() || !(TileSpanM_ > 0.0)) { return 0.0; }
  const auto across = static_cast<double>(tiles[tile].X - field.CentreX());
  const auto down = static_cast<double>(tiles[tile].Y - field.CentreY());
  return std::sqrt(across * across + down * down) * TileSpanM_;
}

int BuildingField::Build(const GroundQuery &ground,
                         const OsmField &field,
                         std::span<const WayLine> ways) {
  assert(Anchored_);
  AddedFirst_ = static_cast<uint32_t>(Built_.WallCorners.size() + Built_.RoofCorners.size());
  AddedCount_ = 0;

  const std::span<const OsmField::Feature> feats = field.Features();
  if (Mark_.Done(feats)) { return static_cast<int>(Prints_.size()); }

  const int layer = field.Layer(OsmLayer::Buildings);
  const std::span<const double> pts = field.Points();
  const auto firstPrint = static_cast<uint32_t>(Prints_.size());
  int added = 0;

  const TileWatermark::Next next =
      Mark_.Ask(feats,
                field.Tiles(),
                {.CentreX = field.CentreX(), .CentreY = field.CentreY(), .Rings = kEveryRing},
                [&](size_t from, size_t to) {
                  return TileGroundResolved(ground, field, {.From = from, .To = to}, layer);
                });
  if (!next.Found) { return static_cast<int>(Prints_.size()); }
  Mark_.Take(next.Tile);

  const double awayM = AwayFromCentreM(field, next.Tile);
  const double smallestSeenM = awayM > 0.0 && FocalPx_ > 0.0 ? kMassedAtPx * awayM / FocalPx_ : 0.0;

  std::map<uint64_t, Lumped> lumps;
  size_t lumped = 0;

  for (size_t c = next.From; c < next.To; c++) {
    const OsmField::Feature &f = feats[c];
    if (f.Type != 3 || std::cmp_not_equal(f.Layer, layer)) { continue; }
    const double h = field.Num(f, "height", 0.0);

    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) { continue; }

      double seat = 0.0;
      double acrossM = 0.0;
      double lowLat = 0.0;
      double highLat = 0.0;
      double lowLon = 0.0;
      double highLon = 0.0;
      const std::optional<double> stood = RingBase(ground, field, ring, &Corners_, &seat).AslM();
      if (!stood) {
        NoGround_++;
        continue;
      }
      const double base = *stood;

      {
        const std::span<const double> ringPts = field.Points();
        lowLat = kNoLeastYet;
        highLat = -kNoLeastYet;
        lowLon = kNoLeastYet;
        highLon = -kNoLeastYet;
        for (uint32_t k = 0; k < ring.Count; k++) {
          const double atLat = ringPts[2 * (static_cast<size_t>(ring.First) + k)];
          const double atLon = ringPts[2 * (static_cast<size_t>(ring.First) + k) + 1];
          lowLat = std::min(lowLat, atLat);
          highLat = std::max(highLat, atLat);
          lowLon = std::min(lowLon, atLon);
          highLon = std::max(highLon, atLon);
        }
        const double perLonM = 111320.0 * std::cos(0.5 * (lowLat + highLat) * kDeg2Rad);
        SeatSpread_.push_back(seat - base);
        acrossM = std::max((highLat - lowLat) * kMPerDegLat, (highLon - lowLon) * perLonM);
        Across_.push_back(acrossM);
      }

      double standBackM = -1.0;
      const Frontage street = NearestStreet(field, ring, ways, &standBackM);

      Footprint fp{};
      fp.FirstPoint = ring.First;
      fp.PointCount = ring.Count;
      fp.Street = street;
      if (street.Known) { Fronted_++; }
      if (h > 0.0 && std::fabs(h - kFillHeightM) > kSameHeightM) {
        fp.HeightM = static_cast<float>(h);
        fp.Source = HeightSource::Osm;
        OsmHeights_++;
      } else {
        const int storeys =
            DefaultStoreys({.AreaM2 = RingAreaM2(field, ring),
                            .AcrossM = AcrossM(field, ring),
                            .StandBackM = standBackM},
                           {.LongitudeDeg = pts[static_cast<size_t>(ring.First) * 2 + 1],
                            .LatitudeDeg = pts[static_cast<size_t>(ring.First) * 2]});
        fp.HeightM = static_cast<float>(static_cast<double>(storeys) * kStoreyM + kRoofAllowanceM);
        fp.Source = HeightSource::Default;
        DefaultHeights_++;
      }
      fp.BaseM = static_cast<float>(base);
      fp.FootM = static_cast<float>(base);
      fp.SeatM = static_cast<float>(seat);
      Prints_.push_back(fp);

      if (acrossM > 0.0 && acrossM < smallestSeenM) {
        Lump(lumps,
             {.LowLat = lowLat, .HighLat = highLat, .LowLon = lowLon, .HighLon = highLon},
             {.BaseM = base, .SeatM = seat, .HeightM = fp.HeightM},
             smallestSeenM);
        ++lumped;
        added++;
        continue;
      }

      const auto meshFrom = std::chrono::steady_clock::now();
      Raise(field, fp);
      MeshMs_ +=
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - meshFrom)
              .count();
      added++;
    }
  }

  ByTile_.Set(next.Tile, firstPrint, static_cast<uint32_t>(Prints_.size()));
  Mark_.Advance(feats);

  AddedCount_ =
      static_cast<uint32_t>(Built_.WallCorners.size() + Built_.RoofCorners.size()) - AddedFirst_;
  if (added == 0) { return static_cast<int>(Prints_.size()); }

  Log::Info(LogTag::World,
            "buildings",
            {{"added", added},
             {"total", static_cast<int>(Prints_.size())},
             {"osmHeight", OsmHeights_},
             {"defaultHeight", DefaultHeights_},
             {"vertsMB", static_cast<double>(Built_.HeapBytes()) / kMicroDegree},
             {"noGround", NoGround_},
             {"onStreet", Fronted_},
             {"deferrals", Mark_.Deferrals()},
             {"builtAhead", static_cast<int>(Mark_.AheadCount())},
             {"lumped", static_cast<int>(lumped)},
             {"blocks", static_cast<int>(lumps.size())},
             {"awayKm", awayM / kMPerKm},
             {"meshMs", MeshMs_}});
  return static_cast<int>(Prints_.size());
}

void BuildingField::RaiseLump(const BuildingField::Lumped &of) {
  if (Mesher_ == nullptr || of.Count == 0) { return; }
  const auto over = static_cast<double>(of.Count);
  const std::array<double, 8> ring = {
      of.LowLat, of.LowLon, of.LowLat, of.HighLon, of.HighLat, of.HighLon, of.HighLat, of.LowLon};
  Corners_.assign(4, of.BaseSum / over);
  StructurePlan plan;
  plan.RingLatLon = std::span<const double>(ring.data(), ring.size());
  plan.BaseAslM = of.BaseSum / over;
  plan.SeatAslM = of.SeatSum / over;
  plan.FootAslM = of.BaseSum / over;
  plan.CornerAslM = std::span<const double>(Corners_.data(), Corners_.size());
  plan.HeightM = of.HeightSum / over;
  plan.HeightMeasured = false;
  plan.Street = {};
  plan.AnchorEcef = Anchor_;
  plan.FocalPx = FocalPx_;
  (void)Mesher_->Mesh(plan, Built_);
}

void BuildingField::Raise(const OsmField &field, const Footprint &f) {
  if (Mesher_ == nullptr) { return; }
  const std::span<const double> pts = field.Points();
  StructurePlan plan;
  plan.RingLatLon = std::span<const double>(pts.data() + static_cast<size_t>(f.FirstPoint) * 2,
                                            static_cast<size_t>(f.PointCount) * 2);
  plan.BaseAslM = f.BaseM;
  plan.SeatAslM = f.SeatM;
  plan.FootAslM = f.FootM;
  plan.CornerAslM = std::span<const double>(Corners_.data(), Corners_.size());
  plan.HeightM = f.HeightM;
  plan.HeightMeasured = f.Source == HeightSource::Osm;
  plan.Street = f.Street;
  plan.AnchorEcef = Anchor_;
  plan.FocalPx = FocalPx_;
  if (!Mesher_->Mesh(plan, Built_)) { return; }
}

} // namespace outshine::Ground

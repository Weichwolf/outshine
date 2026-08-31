#include "BuildingField.h"

#include <chrono>

#include "Geodesy.h"
#include "Log.h"
#include "TerrainLoader.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <numbers>

namespace outshine::Ground {

namespace {

constexpr double kFillHeightM = 5.0;

constexpr double kStoreyM = 2.9;
constexpr double kRoofAllowanceM = 3.2;

constexpr double kOnTheStreetM = 16.0;

constexpr double kCarriagewayM = 4.0;

uint32_t PlaceHash(double latDeg, double lonDeg) {
  uint32_t h = (uint32_t)(int32_t)std::llround(latDeg * 1.0e6) * 2654435761u;
  h ^= (uint32_t)(int32_t)std::llround(lonDeg * 1.0e6) * 2246822519u;
  h ^= h >> 13;
  h *= 3266489917u;
  return h ^ (h >> 16);
}

int DefaultStoreys(double areaM2, double acrossM, double standBackM, double latDeg, double lonDeg) {
  const uint32_t h = PlaceHash(latDeg, lonDeg);
  const bool onStreet = standBackM >= 0.0 && standBackM <= kOnTheStreetM;

  const bool aPlot = areaM2 >= 70.0;
  int least = 1;
  int most = 2;
  if (onStreet && aPlot && acrossM <= 14.0) {
    least = 3;
    most = 5;
  } else if (onStreet && aPlot) {
    least = 2;
    most = 4;
  } else if (!aPlot) {
    least = 1;
    most = 2;
  } else if (acrossM > 26.0) {
    least = 1;
    most = 2;
  } else {
    least = 1;
    most = 3;
  }
  return least + (int)(h % (uint32_t)(most - least + 1));
}

double RingAreaM2(const OsmField &field, const OsmField::Ring &ring) {
  const std::span<const double> pts = field.Points();
  const double refLat = pts[(size_t)ring.First * 2];
  const double refLon = pts[(size_t)ring.First * 2 + 1];
  double a = 0.0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const uint32_t j = (k + 1) % ring.Count;
    double ek = 0.0;
    double nk = 0.0;
    double ej = 0.0;
    double nj = 0.0;
    EnuOffsetM(refLat,
               refLon,
               pts[((size_t)ring.First + k) * 2],
               pts[((size_t)ring.First + k) * 2 + 1],
               ek,
               nk);
    EnuOffsetM(refLat,
               refLon,
               pts[((size_t)ring.First + j) * 2],
               pts[((size_t)ring.First + j) * 2 + 1],
               ej,
               nj);
    a += ek * nj - ej * nk;
  }
  return std::fabs(0.5 * a);
}

double AcrossM(const OsmField &field, const OsmField::Ring &ring) {
  const std::span<const double> pts = field.Points();
  const double refLat = pts[(size_t)ring.First * 2];
  const double refLon = pts[(size_t)ring.First * 2 + 1];
  double e0 = 1e30;
  double e1 = -1e30;
  double n0 = 1e30;
  double n1 = -1e30;
  for (uint32_t k = 0; k < ring.Count; k++) {
    double e = 0.0;
    double n = 0.0;
    EnuOffsetM(refLat,
               refLon,
               pts[((size_t)ring.First + k) * 2],
               pts[((size_t)ring.First + k) * 2 + 1],
               e,
               n);
    e0 = std::min(e0, e);
    e1 = std::max(e1, e);
    n0 = std::min(n0, n);
    n1 = std::max(n1, n);
  }
  return std::min(e1 - e0, n1 - n0);
}

Frontage NearestStreet(const OsmField &field,
                       const OsmField::Ring &ring,
                       Span<const WayLine> ways,
                       double *standBackM) {
  Frontage out;
  *standBackM = -1.0;
  const std::span<const double> pts = field.Points();
  const double refLat = pts[(size_t)ring.First * 2];
  const double refLon = pts[(size_t)ring.First * 2 + 1];
  double cE = 0.0;
  double cN = 0.0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    double e = 0.0;
    double n = 0.0;
    EnuOffsetM(refLat,
               refLon,
               pts[((size_t)ring.First + k) * 2],
               pts[((size_t)ring.First + k) * 2 + 1],
               e,
               n);
    cE += e;
    cN += n;
  }
  cE /= (double)ring.Count;
  cN /= (double)ring.Count;

  const double padDeg = (kOnTheStreetM + 60.0) / 111000.0;
  double best = 1.0e30;
  double bE = 0.0;
  double bN = 0.0;
  double bDirE = 0.0;
  double bDirN = 0.0;
  double bHalf = 0.0;
  for (const WayLine &w : ways) {
    if (w.HalfWidthM * 2.0 < kCarriagewayM) { continue; }
    if (refLat < w.MinLat - padDeg || refLat > w.MaxLat + padDeg) { continue; }
    if (refLon < w.MinLon - padDeg || refLon > w.MaxLon + padDeg) { continue; }
    for (size_t k = 0; k + 3 < w.LatLon.Size(); k += 2) {
      double aE = 0.0;
      double aN = 0.0;
      double bE2 = 0.0;
      double bN2 = 0.0;
      EnuOffsetM(refLat, refLon, w.LatLon[k], w.LatLon[k + 1], aE, aN);
      EnuOffsetM(refLat, refLon, w.LatLon[k + 2], w.LatLon[k + 3], bE2, bN2);
      const double dE = bE2 - aE;
      const double dN = bN2 - aN;
      const double len2 = dE * dE + dN * dN;
      if (len2 < 1.0e-6) { continue; }
      double t = ((cE - aE) * dE + (cN - aN) * dN) / len2;
      t = t < 0.0 ? 0.0 : (t > 1.0 ? 1.0 : t);
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
  if (best > 1.0e29 || best <= bHalf) { return out; }

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
    const double kLat = pts[((size_t)ring.First + k) * 2];
    const double kLon = pts[((size_t)ring.First + k) * 2 + 1];
    const double jLat = pts[((size_t)ring.First + j) * 2];
    const double jLon = pts[((size_t)ring.First + j) * 2 + 1];
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
                                     double *highestAslM) {
  const std::span<const double> pts = field.Points();
  if (corners) { corners->clear(); }
  if (highestAslM != nullptr) { *highestAslM = 0.0; }
  if (ring.Count == 0) { return GroundSample::Missing(); }
  double lowest = 1.0e9;
  double highest = -1.0e9;
  double southest = 1.0e9;
  double northest = -1.0e9;
  double westest = 1.0e9;
  double eastest = -1.0e9;
  int coarsest = 0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const double lat = pts[((size_t)ring.First + k) * 2];
    const double lon = pts[((size_t)ring.First + k) * 2 + 1];
    const GroundSample g = ground.At(lat, lon);
    double aslM = 0.0;
    if (!g.TryAslM(&aslM)) { return g; }
    if (corners) { corners->push_back(aslM); }
    lowest = std::min(lowest, aslM);
    highest = std::max(highest, aslM);
    southest = std::min(southest, lat);
    northest = std::max(northest, lat);
    westest = std::min(westest, lon);
    eastest = std::max(eastest, lon);
    coarsest = std::max(coarsest, g.CoarseBy());
  }

  const double tall = (northest - southest) * kMetresPerDegree;
  const double wide = (eastest - westest) * kMetresPerDegree *
                      std::cos(0.5 * (northest + southest) * std::numbers::pi / 180.0);
  if (std::max(tall, wide) >= kInteriorSpanM) {
    for (int row = 1; row < kInteriorGrid; ++row) {
      for (int column = 1; column < kInteriorGrid; ++column) {
        const double lat = southest + (northest - southest) * (double)row / (double)kInteriorGrid;
        const double lon = westest + (eastest - westest) * (double)column / (double)kInteriorGrid;
        if (!InsideRing(pts, ring, lat, lon)) { continue; }
        const GroundSample g = ground.At(lat, lon);
        double aslM = 0.0;
        if (!g.TryAslM(&aslM)) { continue; }
        lowest = std::min(lowest, aslM);
        highest = std::max(highest, aslM);
        coarsest = std::max(coarsest, g.CoarseBy());
      }
    }
  }
  if (highestAslM != nullptr) { *highestAslM = highest; }
  return GroundSample::At(lowest).Coarser(coarsest);
}

bool BuildingField::TileGroundResolved(
    const GroundQuery &ground, const OsmField &field, size_t from, size_t to, int layer) const {
  const std::span<const OsmField::Feature> feats = field.Features();
  for (size_t i = from; i < to; i++) {
    const OsmField::Feature &f = feats[i];
    if (f.Type != 3 || (int)f.Layer != layer) { continue; }
    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) { continue; }
      const GroundSample base = RingBase(ground, field, ring, nullptr);
      if (base.Where() == GroundSample::State::Pending || base.CoarseBy() > 0) { return false; }
    }
  }
  return true;
}

void BuildingField::AnchorAt(const double ecef[3]) {
  assert(Prints_.empty());
  for (int c = 0; c < 3; c++) { Anchor_[c] = ecef[c]; }
  Anchored_ = true;
}

int BuildingField::Build(const GroundQuery &ground,
                         const OsmField &field,
                         Span<const WayLine> ways) {
  assert(Anchored_);
  AddedFirst_ = (uint32_t)(Built_.WallCorners.size() + Built_.RoofCorners.size());
  AddedCount_ = 0;

  const std::span<const OsmField::Feature> feats = field.Features();
  if (Mark_.Done(feats)) { return (int)Prints_.size(); }

  const int layer = field.Layer(OsmLayer::Buildings);
  const std::span<const double> pts = field.Points();
  const uint32_t firstPrint = (uint32_t)Prints_.size();
  int added = 0;

  const TileWatermark::Next next = Mark_.Ask(
      feats, field.Tiles(), field.CentreX(), field.CentreY(), [&](size_t from, size_t to) {
        return TileGroundResolved(ground, field, from, to, layer);
      });
  if (!next.Found) { return (int)Prints_.size(); }
  Mark_.Take(next.Tile);

  for (size_t c = next.From; c < next.To; c++) {
    const OsmField::Feature &f = feats[c];
    if (f.Type != 3 || (int)f.Layer != layer) { continue; }
    const double h = field.Num(f, "height", 0.0);

    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) { continue; }

      double base = 0.0;
      double seat = 0.0;
      if (!RingBase(ground, field, ring, &Corners_, &seat).TryAslM(&base)) {
        NoGround_++;
        continue;
      }

      double standBackM = -1.0;
      const Frontage street = NearestStreet(field, ring, ways, &standBackM);

      Footprint fp{};
      fp.FirstPoint = ring.First;
      fp.PointCount = ring.Count;
      fp.Street = street;
      if (street.Known) { Fronted_++; }
      if (h > 0.0 && std::fabs(h - kFillHeightM) > 0.01) {
        fp.HeightM = (float)h;
        fp.Source = HeightSource::Osm;
        OsmHeights_++;
      } else {
        const int storeys = DefaultStoreys(RingAreaM2(field, ring),
                                           AcrossM(field, ring),
                                           standBackM,
                                           pts[(size_t)ring.First * 2],
                                           pts[(size_t)ring.First * 2 + 1]);
        fp.HeightM = (float)((double)storeys * kStoreyM + kRoofAllowanceM);
        fp.Source = HeightSource::Default;
        DefaultHeights_++;
      }
      fp.BaseM = (float)base;
      fp.FootM = (float)base;
      fp.SeatM = (float)seat;
      Prints_.push_back(fp);
      const auto meshFrom = std::chrono::steady_clock::now();
      Raise(field, fp);
      MeshMs_ +=
          std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - meshFrom)
              .count();
      added++;
    }
  }

  ByTile_.Set(next.Tile, firstPrint, (uint32_t)Prints_.size());
  Mark_.Advance(feats);

  AddedCount_ = (uint32_t)(Built_.WallCorners.size() + Built_.RoofCorners.size()) - AddedFirst_;
  if (added == 0) { return (int)Prints_.size(); }

  Log::Info("world",
            "buildings",
            {{"added", added},
             {"total", (int)Prints_.size()},
             {"osmHeight", OsmHeights_},
             {"defaultHeight", DefaultHeights_},
             {"vertsMB", (double)Built_.HeapBytes() / 1.0e6},
             {"noGround", NoGround_},
             {"onStreet", Fronted_},
             {"deferrals", Mark_.Deferrals()},
             {"builtAhead", (int)Mark_.AheadCount()},
             {"meshMs", MeshMs_}});
  return (int)Prints_.size();
}

void BuildingField::Raise(const OsmField &field, const Footprint &f) {
  if (!Mesher_) { return; }
  const std::span<const double> pts = field.Points();
  StructurePlan plan;
  plan.RingLatLon =
      Span<const double>(pts.data() + (size_t)f.FirstPoint * 2, (size_t)f.PointCount * 2);
  plan.BaseAslM = f.BaseM;
  plan.SeatAslM = f.SeatM;
  plan.FootAslM = f.FootM;
  plan.CornerAslM = Span<const double>(Corners_.data(), Corners_.size());
  plan.HeightM = f.HeightM;
  plan.HeightMeasured = f.Source == HeightSource::Osm;
  plan.Street = f.Street;
  plan.AnchorEcef = Anchor_;
  plan.FocalPx = FocalPx_;
  Mesher_->Mesh(plan, Built_);
}

} // namespace outshine::Ground

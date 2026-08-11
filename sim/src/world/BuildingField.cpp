#include "BuildingField.h"

#include "Geodesy.h"
#include "Log.h"
#include "TerrainLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace outshine::World {

namespace {

/* THE PROVIDER'S FILL, not a building's height. Measured on /t/vector/14/8617/5404 (Hameln): 1634 of
 * 1706 footprints carry exactly 5 — 95.8 %, integer, and the Altstadt they cover is three and four
 * storeys of half-timber. A single value on 96 % of a town is a default, and taking it literally puts
 * the whole of Hameln under a caravan roof. */
constexpr double kFillHeightM = 5.0;

/* [SET] the storey and the roof the default is built out of: 2.9 m floor-to-floor is the German
 * residential measure, 3.2 m is a 35 deg pitched roof on a ~9 m span. The right fix is upstream —
 * fb-tiles must carry OSM's building:levels — and these exist to be deleted when it does. */
constexpr double kStoreyM = 2.9;
constexpr double kRoofAllowanceM = 3.2;

/* HOW MANY STOREYS A PLAN OF THIS SIZE CARRIES, where nobody said. A single number over a whole town
 * is what puts a terrace, a garage and a department store under one eaves line, and the plan area is
 * the one piece of evidence the source does supply. The steps are the German small-town census
 * pattern: an outbuilding is one storey, a house two, a town-centre plot three, a plot over 600 m2
 * four. A DETERMINISTIC step of one either way comes off the corner's own position, because a row of
 * identical eaves is a statement about the data that is not true either. */
int DefaultStoreys(double areaM2, double latDeg, double lonDeg) {
  int storeys = areaM2 < 30.0 ? 1 : areaM2 < 150.0 ? 2 : areaM2 < 600.0 ? 3 : 4;
  uint32_t h = (uint32_t)(int32_t)std::llround(latDeg * 1.0e6) * 2654435761u;
  h ^= (uint32_t)(int32_t)std::llround(lonDeg * 1.0e6) * 2246822519u;
  h ^= h >> 13;
  h *= 3266489917u;
  h ^= h >> 16;
  if (storeys > 1) storeys += (int)(h % 4u) - 1;   /* -1, 0, 0, +1 */
  return std::max(1, storeys);
}

/* The plan area of the ring, in metres, off the same corners the wall will stand on. */
double RingAreaM2(const OsmField &field, const OsmField::Ring &ring) {
  const std::vector<double> &pts = field.Points();
  const double refLat = pts[(size_t)ring.First * 2], refLon = pts[(size_t)ring.First * 2 + 1];
  double a = 0.0;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const uint32_t j = (k + 1) % ring.Count;
    double ek = 0.0, nk = 0.0, ej = 0.0, nj = 0.0;
    EnuOffsetM(refLat, refLon, pts[((size_t)ring.First + k) * 2],
               pts[((size_t)ring.First + k) * 2 + 1], ek, nk);
    EnuOffsetM(refLat, refLon, pts[((size_t)ring.First + j) * 2],
               pts[((size_t)ring.First + j) * 2 + 1], ej, nj);
    a += ek * nj - ej * nk;
  }
  return std::fabs(0.5 * a);
}

}  // namespace

/* The lowest corner of a ring, or why there is none. Read TWICE per tile — once to decide whether the
 * tile can be consumed at all, once to build — because the second read is a hit in the oracle's own
 * tile cache while the first is what keeps a footprint from being thrown away for arriving early. */
GroundSample BuildingField::RingBase(const OsmField &field, const OsmField::Ring &ring) {
  const std::vector<double> &pts = field.Points();
  if (ring.Count == 0) return GroundSample::Missing();
  double lowest = 1.0e9;
  for (uint32_t k = 0; k < ring.Count; k++) {
    const GroundSample g = fb_stream_ground(pts[((size_t)ring.First + k) * 2],
                                            pts[((size_t)ring.First + k) * 2 + 1]);
    double aslM = 0.0;
    if (!g.TryAslM(&aslM)) return g;   /* the corner's own reason travels out, unre-encoded */
    lowest = std::min(lowest, aslM);
  }
  return GroundSample::At(lowest);
}

bool BuildingField::TileGroundResolved(const OsmField &field, size_t from, size_t to,
                                       int layer) const {
  const std::vector<OsmField::Feature> &feats = field.Features();
  for (size_t i = from; i < to; i++) {
    const OsmField::Feature &f = feats[i];
    if (f.Type != 3 || (int)f.Layer != layer) continue;
    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) continue;
      if (RingBase(field, ring).Where() == GroundSample::State::Pending) return false;
    }
  }
  return true;
}

int BuildingField::Build(const OsmField &field) {
  AddedFirst_ = (uint32_t)Verts_.size();
  AddedCount_ = 0;

  const std::vector<OsmField::Feature> &feats = field.Features();
  if (Mark_.Done(feats)) return (int)Prints_.size();

  const int layer = field.Layer(OsmLayer::Buildings);
  const std::vector<double> &pts = field.Points();
  const uint32_t firstPrint = (uint32_t)Prints_.size();
  int added = 0;

  /* A FOOTPRINT IS CONSUMED ONCE. Whichever ring is asked first, a tile is either buildable now or
   * asked again next pass — dropping a stand because its DEM had not landed yet is indistinguishable
   * from "nothing stands here" and is never revisited. */
  const TileWatermark::Next next = Mark_.Ask(feats, [&](size_t from, size_t to) {
    return TileGroundResolved(field, from, to, layer);
  });
  if (!next.Found) return (int)Prints_.size();
  Mark_.Take(next.Tile);

  for (size_t c = next.From; c < next.To; c++) {
    const OsmField::Feature &f = feats[c];
    if (f.Type != 3 || (int)f.Layer != layer) continue;
    const double h = field.Num(f, "height", 0.0);

    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (!ring.Exterior || ring.Count < 3 || ring.Count > 512) continue;

      double base = 0.0;
      if (!RingBase(field, ring).TryAslM(&base)) { NoGround_++; continue; }

      Footprint fp{};
      fp.FirstPoint = ring.First;
      fp.PointCount = ring.Count;
      if (h > 0.0 && std::fabs(h - kFillHeightM) > 0.01) {
        fp.HeightM = (float)h;
        fp.Source = HeightSource::Osm;
        OsmHeights_++;
      } else {
        const int storeys = DefaultStoreys(RingAreaM2(field, ring), pts[(size_t)ring.First * 2],
                                           pts[(size_t)ring.First * 2 + 1]);
        fp.HeightM = (float)((double)storeys * kStoreyM + kRoofAllowanceM);
        fp.Source = HeightSource::Default;
        DefaultHeights_++;
      }
      fp.BaseM = (float)base;
      if (!HaveAnchor_) {
        GeoToEcef(pts[(size_t)fp.FirstPoint * 2], pts[(size_t)fp.FirstPoint * 2 + 1], base, Anchor_);
        HaveAnchor_ = true;
      }
      Prints_.push_back(fp);
      Raise(field, fp);
      added++;
    }
  }

  ByTile_.Set(next.Tile, firstPrint, (uint32_t)Prints_.size());
  Mark_.Advance(feats);

  AddedCount_ = (uint32_t)Verts_.size() - AddedFirst_;
  if (added == 0) return (int)Prints_.size();

  Log::Info("world", "buildings", {{"added", added}, {"total", (int)Prints_.size()},
                                     {"osmHeight", OsmHeights_}, {"defaultHeight", DefaultHeights_},
                                     {"vertsMB", (double)Verts_.size() * 4.0 / 1.0e6},
                                     {"noGround", NoGround_}, {"deferrals", Mark_.Deferrals()},
                                     {"builtAhead", (int)Mark_.AheadCount()}});
  return (int)Prints_.size();
}

/* WHAT STANDS ON THE OUTLINE is not decided here. This resolves the ring, the ground under it and a
 * height; the shape of the thing is a generator's, installed from above, and the server target
 * installs none — which is why a machine with no device no longer extrudes a town it cannot draw. */
void BuildingField::Raise(const OsmField &field, const Footprint &f) {
  if (!Mesher_) return;
  const std::vector<double> &pts = field.Points();
  StructurePlan plan;
  plan.RingLatLon = Span<const double>(pts.data() + (size_t)f.FirstPoint * 2, (size_t)f.PointCount * 2);
  plan.BaseAslM = f.BaseM;
  plan.HeightM = f.HeightM;
  plan.HeightMeasured = f.Source == HeightSource::Osm;
  plan.AnchorEcef = Anchor_;
  Mesher_->Mesh(plan, Verts_);
}

} // namespace outshine::World

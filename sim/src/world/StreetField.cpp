#include "StreetField.h"

namespace outshine::World {

namespace {

/* An OSM way arrives clipped at the tile border, so a two-point remnant is normal; the cap is the
 * same one the other producers use against a ring that would blow the per-feature scratch. */
constexpr uint32_t kMaxRingPoints = 512;

}  // namespace

uint32_t StreetField::Ingest(const OsmField &field, const VegetationTemplates &veg) {
  const std::vector<OsmField::Feature> &feats = field.Features();
  if (Mark_.Done(feats)) return (uint32_t)Ways_.size();

  /* Nothing here has to wait for a height, so every tile is consumable the moment it is decoded. */
  const TileWatermark::Next next = Mark_.Ask(feats, [](size_t, size_t) { return true; });
  if (!next.Found) return (uint32_t)Ways_.size();
  Mark_.Take(next.Tile);
  Mark_.Advance(feats);

  const int lines = field.Layer(OsmLayer::Streets);
  const int areas = field.Layer(OsmLayer::StreetPolygons);
  const uint32_t firstWay = (uint32_t)Ways_.size();

  for (size_t c = next.From; c < next.To; c++) {
    const OsmField::Feature &f = feats[c];
    const bool ribbon = f.Type == 2 && (int)f.Layer == lines;
    const bool area = f.Type == 3 && (int)f.Layer == areas;
    if (!ribbon && !area) continue;
    /* A tunnel is not a surface. The class grid drops one for the same reason and this must agree
     * with it, or a query and the ground it stands on answer differently at the same point. */
    if (field.Num(f, "tunnel", 0.0) > 0.5) { Tunnels_++; continue; }

    const VegetationTemplates::Rule *rule =
        veg.Find(field.LayerName((int)f.Layer), field.Str(f, "kind"));
    if (!rule) continue;   /* an undeclared kind is the class field's error to raise, once */
    if (ribbon && rule->WidthM <= 0.0f) { Unwidthed_++; continue; }

    for (uint32_t r = 0; r < f.RingCount; r++) {
      const OsmField::Ring &ring = field.Rings()[f.FirstRing + r];
      if (ring.Count > kMaxRingPoints) continue;
      if (ribbon && ring.Count < 2) continue;
      if (area && (!ring.Exterior || ring.Count < 3)) continue;

      Way w{};
      w.FirstPoint = ring.First;
      w.PointCount = ring.Count;
      w.HalfWidthM = ribbon ? rule->WidthM * 0.5f : 0.0f;
      w.CoverRow = (int32_t)rule->Tpl;
      w.Form = ribbon ? Shape::Ribbon : Shape::Area;
      Ways_.push_back(w);
    }
  }

  ByTile_.Set(next.Tile, firstWay, (uint32_t)Ways_.size());
  return (uint32_t)Ways_.size();
}

} // namespace outshine::World

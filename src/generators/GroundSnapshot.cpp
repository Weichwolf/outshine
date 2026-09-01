#include "GroundSnapshot.h"

#include <vector>

#include "GroundPatch.h"
#include "GroundSample.h"
#include "Span.h"
#include "VegetationTemplates.h"

namespace outshine::Generators {

std::shared_ptr<const GroundTable> TableOf(const outshine::Ground::VegetationTemplates &templates) {
  const outshine::Ground::VegetationTemplates::Row *rows = templates.Rows();
  const size_t count = templates.TemplateCount();
  std::vector<GroundTable::Row> table(count);
  for (size_t at = 0; at < count; ++at) {
    for (int channel = 0; channel < 3; ++channel) {
      table[at].Surface.BaseColour[channel] = rows[at].Ground[channel];
    }
    table[at].Surface.Roughness = rows[at].Ground[3];
    table[at].SlopeMaxDeg = rows[at].Edge[3];
  }
  return GroundTable::Of(Span<const GroundTable::Row>(table.data(), table.size()));
}

std::shared_ptr<const FeatureField> FeaturesOver(const Tile &region, const Fields &stands) {
  if (stands.Vectors == nullptr || stands.Footprints == nullptr || stands.WaterBodies == nullptr ||
      stands.Ways == nullptr) {
    return nullptr;
  }
  if (!stands.Vectors->Settled(region.X(), region.Y())) { return nullptr; }
  const int tile = stands.Vectors->TileIndex(region.X(), region.Y());
  const std::span<const double> points = stands.Vectors->Points();

  std::vector<FeatureField::Feature> features;
  std::vector<FeatureField::Ring> rings;
  std::vector<FeatureField::Vertex> vertices;
  const auto take = [&](const FeatureField::Feature &proto, uint32_t firstPoint, uint32_t count) {
    const uint32_t least = proto.Form == FeatureForm::Ribbon ? 2u : 3u;
    if (count < least) { return; }
    FeatureField::Feature f = proto;
    f.FirstRing = static_cast<uint32_t>(rings.size());
    f.RingCount = 1;
    rings.push_back({.First = static_cast<uint32_t>(vertices.size()), .Count = count});
    for (uint32_t k = 0; k < count; k++) {
      double eastM = 0.0;
      double northM = 0.0;
      region.Enu(points[(static_cast<size_t>(firstPoint) + k) * 2],
                 points[(static_cast<size_t>(firstPoint) + k) * 2 + 1],
                 &eastM,
                 &northM);
      vertices.push_back({.Em = static_cast<float>(eastM), .Nm = static_cast<float>(northM)});
    }
    features.push_back(f);
  };

  for (const outshine::Ground::BuildingField::Footprint &fp : stands.Footprints->OfTile(tile)) {
    FeatureField::Feature f{};
    f.CoverRow = stands.BuiltRow;
    f.Kind = FeatureKind::Structure;
    f.Form = FeatureForm::Area;
    f.Base = FeatureLevel::At(fp.BaseM);
    f.Top = FeatureLevel::At(fp.BaseM + fp.HeightM);
    take(f, fp.FirstPoint, fp.PointCount);
  }
  for (const outshine::Ground::WaterField::Surface &s : stands.WaterBodies->OfTile(tile)) {
    FeatureField::Feature f{};
    f.CoverRow = stands.WetRow;
    f.Kind = FeatureKind::Water;
    f.Form = FeatureForm::Area;
    f.Top = FeatureLevel::At(s.LevelM);
    take(f, s.FirstPoint, s.PointCount);
  }
  for (const outshine::Ground::StreetField::Way &w : stands.Ways->OfTile(tile)) {
    FeatureField::Feature f{};
    f.CoverRow = w.CoverRow;
    f.Kind = FeatureKind::Way;
    f.Form = w.Form == outshine::Ground::StreetField::Shape::Ribbon ? FeatureForm::Ribbon
                                                                    : FeatureForm::Area;
    f.HalfWidthM = w.HalfWidthM;
    take(f, w.FirstPoint, w.PointCount);
  }

  return FeatureField::Of(Span<const FeatureField::Feature>(features.data(), features.size()),
                          Span<const FeatureField::Ring>(rings.data(), rings.size()),
                          Span<const FeatureField::Vertex>(vertices.data(), vertices.size()));
}

Snapped SnapshotOver(const Tile &region,
                     const outshine::GroundQuery &heights,
                     const outshine::Ground::ClassField &classes,
                     const Fields &stands,
                     std::shared_ptr<const GroundTable> table,
                     Ground::Snapshot *out) {
  const auto done = [](Snapped how) { return how; };
  const int blockZoom = heights.BlockZoom() > 0 ? heights.BlockZoom() : region.Zoom();
  const int coarser = region.Zoom() - blockZoom;
  const outshine::Ground::GroundBlock block =
      coarser > 0 ? heights.BlockAt(blockZoom, region.X() >> coarser, region.Y() >> coarser)
                  : heights.BlockAt(blockZoom, region.X(), region.Y());
  switch (block.Where()) {
    case outshine::Ground::GroundBlock::State::Pending: return done(Snapped::Waiting);
    case outshine::Ground::GroundBlock::State::Missing: return done(Snapped::NoGround);
    case outshine::Ground::GroundBlock::State::Resolved: break;
  }

  const int side = static_cast<int>(region.SpanNm() / heights.PostM(region.AnchorLat()) + 0.5) + 1;
  std::vector<GroundPatch::Posting> postings(static_cast<size_t>(side) * static_cast<size_t>(side));
  std::vector<double> row(static_cast<size_t>(side));
  const double stepE = region.SpanEm() / static_cast<double>(side - 1);
  const double stepN = region.SpanNm() / static_cast<double>(side - 1);
  for (int j = 0; j < side; j++) {
    double lat = 0.0;
    double lonFrom = 0.0;
    double latAgain = 0.0;
    double lonNext = 0.0;
    region.Geo(0.0, static_cast<double>(j) * stepN, &lat, &lonFrom);
    region.Geo(stepE, static_cast<double>(j) * stepN, &latAgain, &lonNext);
    block.AslMRow(lat, lonFrom, lonNext - lonFrom, side, row.data());
    for (int i = 0; i < side; i++) {
      postings[static_cast<size_t>(j) * static_cast<size_t>(side) + static_cast<size_t>(i)].Height =
          GroundSample::At(row[static_cast<size_t>(i)]);
    }
  }
  out->Patch = GroundPatch::Complete(
      region, side, Span<const GroundPatch::Posting>(postings.data(), postings.size()));
  out->Classes = classes.Read();
  out->Features = FeaturesOver(region, stands);
  out->Table = std::move(table);
  return done(out->Patch && out->Classes && out->Features ? Snapped::Taken : Snapped::Waiting);
}

} // namespace outshine::Generators

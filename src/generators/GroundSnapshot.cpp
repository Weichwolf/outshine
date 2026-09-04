#include "GroundSnapshot.h"

#include <memory>
#include <cmath>
#include <cstddef>
#include <span>
#include <cstdint>
#include <utility>
#include <vector>

#include "GroundPatch.h"
#include "GroundSample.h"
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
  return GroundTable::Of(std::span<const GroundTable::Row>(table.data(), table.size()));
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
  const auto take = [&](const FeatureField::Feature &proto, FeatureField::Ring over) {
    const uint32_t least = proto.Form == FeatureForm::Ribbon ? 2u : 3u;
    if (over.Count < least) { return; }
    FeatureField::Feature f = proto;
    f.FirstRing = static_cast<uint32_t>(rings.size());
    f.RingCount = 1;
    rings.push_back({.First = static_cast<uint32_t>(vertices.size()), .Count = over.Count});
    for (uint32_t k = 0; k < over.Count; k++) {
      const EastNorth on =
          region.Enu({.LongitudeDeg = points[(static_cast<size_t>(over.First) + k) * 2 + 1],
                      .LatitudeDeg = points[(static_cast<size_t>(over.First) + k) * 2]});
      vertices.push_back({.Em = static_cast<float>(on.EastM), .Nm = static_cast<float>(on.NorthM)});
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
    take(f, {.First = fp.FirstPoint, .Count = fp.PointCount});
  }
  for (const outshine::Ground::WaterField::Surface &s : stands.WaterBodies->OfTile(tile)) {
    FeatureField::Feature f{};
    f.CoverRow = stands.WetRow;
    f.Kind = FeatureKind::Water;
    f.Form = FeatureForm::Area;
    f.Top = FeatureLevel::At(s.LevelM);
    take(f, {.First = s.FirstPoint, .Count = s.PointCount});
  }
  for (const outshine::Ground::StreetField::Way &w : stands.Ways->OfTile(tile)) {
    FeatureField::Feature f{};
    f.CoverRow = w.CoverRow;
    f.Kind = FeatureKind::Way;
    f.Form = w.Form == outshine::Ground::StreetField::Shape::Ribbon ? FeatureForm::Ribbon
                                                                    : FeatureForm::Area;
    f.HalfWidthM = w.HalfWidthM;
    take(f, {.First = w.FirstPoint, .Count = w.PointCount});
  }

  return FeatureField::Of(std::span<const FeatureField::Feature>(features.data(), features.size()),
                          std::span<const FeatureField::Ring>(rings.data(), rings.size()),
                          std::span<const FeatureField::Vertex>(vertices.data(), vertices.size()));
}

std::shared_ptr<const GroundPatch>
PatchOver(const Tile &region, const outshine::GroundQuery &heights, Snapped *how) {
  const int blockZoom = heights.BlockZoom() > 0 ? heights.BlockZoom() : region.Zoom();
  const int coarser = region.Zoom() - blockZoom;
  const outshine::Ground::GroundBlock block =
      coarser > 0 ? heights.BlockAt({.Zoom = blockZoom,
                                     .X = static_cast<long>(static_cast<uint32_t>(region.X()) >>
                                                            static_cast<uint32_t>(coarser)),
                                     .Y = static_cast<long>(static_cast<uint32_t>(region.Y()) >>
                                                            static_cast<uint32_t>(coarser))})
                  : heights.BlockAt({.Zoom = blockZoom, .X = region.X(), .Y = region.Y()});
  switch (block.Where()) {
    case outshine::Ground::GroundBlock::State::Pending: *how = Snapped::Waiting; return nullptr;
    case outshine::Ground::GroundBlock::State::Missing: *how = Snapped::NoGround; return nullptr;
    case outshine::Ground::GroundBlock::State::Resolved: break;
  }
  const int side =
      static_cast<int>(std::lround(region.SpanNm() / heights.PostM(region.AnchorLat()))) + 1;
  std::vector<GroundPatch::Posting> postings(static_cast<size_t>(side) * static_cast<size_t>(side));
  std::vector<double> row(static_cast<size_t>(side));
  const double stepE = region.SpanEm() / static_cast<double>(side - 1);
  const double stepN = region.SpanNm() / static_cast<double>(side - 1);
  for (int j = 0; j < side; j++) {
    const LongitudeLatitude from =
        region.Geo({.EastM = 0.0, .NorthM = static_cast<double>(j) * stepN});
    const LongitudeLatitude next =
        region.Geo({.EastM = stepE, .NorthM = static_cast<double>(j) * stepN});
    block.AslMRow(from,
                  next.LongitudeDeg - from.LongitudeDeg,
                  std::span<double>(row.data(), static_cast<size_t>(side)));
    for (int i = 0; i < side; i++) {
      postings[static_cast<size_t>(j) * static_cast<size_t>(side) + static_cast<size_t>(i)].Height =
          GroundSample::At(row[static_cast<size_t>(i)]);
    }
  }
  std::shared_ptr<const GroundPatch> patch = GroundPatch::Complete(
      region, side, std::span<const GroundPatch::Posting>(postings.data(), postings.size()));
  *how = patch ? Snapped::Taken : Snapped::Waiting;
  return patch;
}

Snapped SnapshotOver(const Tile &region,
                     const outshine::GroundQuery &heights,
                     const outshine::Ground::ClassField &classes,
                     const Fields &stands,
                     std::shared_ptr<const GroundTable> table,
                     Ground::Snapshot *out) {
  Snapped how = Snapped::Taken;
  out->Patch = PatchOver(region, heights, &how);
  if (!out->Patch) { return how; }
  out->Classes = classes.Read();
  out->Features = FeaturesOver(region, stands);
  out->Table = std::move(table);
  return out->Patch && out->Classes && out->Features ? Snapped::Taken : Snapped::Waiting;
}

} // namespace outshine::Generators

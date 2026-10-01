#include "GroundSnapshot.h"

#include <algorithm>
#include <limits>
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

namespace {

bool Intersects(const Tile &region,
                const outshine::Ground::BuildingField::Footprint &footprint,
                std::span<const double> coordinates) {
  double west = std::numeric_limits<double>::infinity();
  double south = west;
  double east = -west;
  double north = -west;
  for (size_t point = footprint.FirstPoint;
       point < static_cast<size_t>(footprint.FirstPoint) + footprint.PointCount;
       ++point) {
    west = std::min(west, coordinates[point * 2u + 1u]);
    east = std::max(east, coordinates[point * 2u + 1u]);
    south = std::min(south, coordinates[point * 2u]);
    north = std::max(north, coordinates[point * 2u]);
  }
  const auto southwest = region.Geo({.EastM = 0, .NorthM = 0});
  const auto northeast = region.Geo({.EastM = region.SpanEm(), .NorthM = region.SpanNm()});
  return east >= southwest.LongitudeDeg && west <= northeast.LongitudeDeg &&
         north >= southwest.LatitudeDeg && south <= northeast.LatitudeDeg;
}

class FeatureAssembly {
public:
  explicit FeatureAssembly(const Tile &region) : Region_(region) {}

  void AppendRing(FeatureField::Ring ring, std::span<const double> coordinates) {
    Rings_.push_back({.First = static_cast<uint32_t>(Vertices_.size()), .Count = ring.Count});
    for (uint32_t point = 0; point < ring.Count; ++point) {
      const size_t at = (static_cast<size_t>(ring.First) + point) * 2u;
      const EastNorth local =
          Region_.Enu({.LongitudeDeg = coordinates[at + 1u], .LatitudeDeg = coordinates[at]});
      Vertices_.push_back(
          {.Em = static_cast<float>(local.EastM), .Nm = static_cast<float>(local.NorthM)});
    }
  }

  void Take(FeatureField::Feature feature,
            FeatureField::Ring ring,
            std::span<const double> coordinates) {
    const uint32_t least = feature.Form == FeatureForm::Ribbon ? 2u : 3u;
    if (ring.Count < least) { return; }
    feature.FirstRing = static_cast<uint32_t>(Rings_.size());
    feature.RingCount = 1;
    AppendRing(ring, coordinates);
    Features_.push_back(feature);
  }

  void Buildings(const Fields &stands, int tile, std::span<const double> points) {
    const auto accepted = stands.Footprints->AcceptedInputs();
    const auto tiles = stands.Footprints->AcceptedTiles();
    for (size_t product = 0; product < accepted.size(); ++product) {
      const auto *geometry = accepted[product].Coordinates.get();
      const bool original = geometry != nullptr && geometry->Origin.Provenance;
      if (!original && std::cmp_not_equal(tiles[product], tile)) { continue; }
      const auto coordinates =
          geometry != nullptr ? std::span<const double>(geometry->Points) : points;
      for (const auto &footprint : stands.Footprints->OfTile(static_cast<int>(tiles[product]))) {
        if (footprint.PointCount < 3 ||
            (original && !Intersects(Region_, footprint, coordinates))) {
          continue;
        }
        FeatureField::Feature feature{};
        feature.CoverRow = stands.BuiltRow;
        feature.Kind = FeatureKind::Structure;
        feature.Form = FeatureForm::Area;
        feature.Base = FeatureLevel::At(footprint.BaseM + footprint.MinimumHeightM);
        feature.Top = FeatureLevel::At(footprint.BaseM + footprint.HeightM);
        Take(feature, {.First = footprint.FirstPoint, .Count = footprint.PointCount}, coordinates);
        if (geometry == nullptr) { continue; }
        for (const auto &hole :
             std::span(geometry->Rings).subspan(footprint.FirstHole, footprint.HoleCount)) {
          AppendRing({.First = hole.First, .Count = hole.Count}, coordinates);
          ++Features_.back().RingCount;
        }
      }
    }
  }

  void Water(const Fields &stands, int tile, std::span<const double> points) {
    for (const auto &surface : stands.WaterBodies->OfTile(tile)) {
      FeatureField::Feature feature{};
      feature.CoverRow = stands.WetRow;
      feature.Kind = FeatureKind::Water;
      feature.Form = FeatureForm::Area;
      feature.Top = FeatureLevel::At(surface.LevelM);
      feature.FirstRing = static_cast<uint32_t>(Rings_.size());
      const auto rings = stands.WaterBodies->RingsOf(surface);
      feature.RingCount = static_cast<uint32_t>(rings.size());
      for (const auto &ring : rings) {
        AppendRing({.First = ring.FirstPoint, .Count = ring.PointCount}, points);
      }
      Features_.push_back(feature);
    }
  }

  void Ways(const Fields &stands, int tile, std::span<const double> points) {
    for (const auto &way : stands.Ways->OfTile(tile)) {
      FeatureField::Feature feature{};
      feature.CoverRow = way.CoverRow;
      feature.Kind = FeatureKind::Way;
      feature.Form = way.Form == outshine::Ground::StreetField::Shape::Ribbon ? FeatureForm::Ribbon
                                                                              : FeatureForm::Area;
      feature.HalfWidthM = way.HalfWidthM;
      Take(feature, {.First = way.FirstPoint, .Count = way.PointCount}, points);
    }
  }

  [[nodiscard]] std::shared_ptr<const FeatureField> Finish() const {
    return FeatureField::Of(Features_, Rings_, Vertices_);
  }

private:
  const Tile &Region_;
  std::vector<FeatureField::Feature> Features_;
  std::vector<FeatureField::Ring> Rings_;
  std::vector<FeatureField::Vertex> Vertices_;
};

}

std::shared_ptr<const FeatureField> FeaturesOver(const Tile &region, const Fields &stands) {
  if (stands.Footprints == nullptr || stands.WaterBodies == nullptr || stands.Ways == nullptr) {
    return nullptr;
  }
  const bool vectorsReady =
      stands.Vectors != nullptr && stands.Vectors->Settled(region.X(), region.Y());
  const bool native =
      std::ranges::any_of(stands.Footprints->AcceptedInputs(), [](const auto &input) {
        return input.Coordinates && input.Coordinates->Origin.Provenance;
      });
  if (!vectorsReady && !native) { return nullptr; }
  const int tile = vectorsReady ? stands.Vectors->TileIndex(region.X(), region.Y()) : -1;
  const std::span<const double> points =
      vectorsReady ? stands.Vectors->Points() : std::span<const double>{};
  FeatureAssembly assembly(region);
  assembly.Buildings(stands, tile, points);
  assembly.Water(stands, tile, points);
  assembly.Ways(stands, tile, points);
  return assembly.Finish();
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
                     std::shared_ptr<const ClassStructure> classes,
                     const Fields &stands,
                     std::shared_ptr<const GroundTable> table,
                     Ground::Snapshot *out) {
  Snapped how = Snapped::Taken;
  out->Patch = PatchOver(region, heights, &how);
  if (!out->Patch) { return how; }
  out->Classes = std::move(classes);
  out->Features = FeaturesOver(region, stands);
  out->Table = std::move(table);
  return out->Patch && out->Classes && out->Features ? Snapped::Taken : Snapped::Waiting;
}

}

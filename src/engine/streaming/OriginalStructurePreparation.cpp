#include "OriginalStructurePreparation.h"
#include "StructureInput.h"
#include "HeightField.h"

#include "OsmBuildingFootprints.h"
#include "OsmSourceCapture.h"
#include "TangentFrame.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include <expected>
#include <memory>
#include <queue>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <tuple>
#include <vector>

namespace outshine {
namespace {

constexpr long kMaximumHeightBlocks = 64;

struct OriginalTerrainRange {
  long West;
  long East;
  long North;
  long South;
};

std::expected<OriginalTerrainRange, std::string>
FootprintTerrainRange(std::span<const double> points, int zoom, const std::stop_token &stop) {
  const long side = static_cast<long>(std::ldexp(1.0, zoom));
  const auto first =
      Ground::HeightField::SpotOf({.LongitudeDeg = points[1], .LatitudeDeg = points[0]}, zoom);
  OriginalTerrainRange range{.West = first.X, .East = first.X, .North = first.Y, .South = first.Y};
  for (size_t point = 0; point < points.size(); point += 2) {
    if (stop.stop_requested()) { return std::unexpected("original terrain demand canceled"); }
    const auto at = Ground::HeightField::SpotOf(
        {.LongitudeDeg = points[point + 1], .LatitudeDeg = points[point]}, zoom);
    if (at.Y < 0 || at.Y >= side) {
      return std::unexpected("original building lies outside the terrain grid");
    }
    long column = at.X;
    if (column - first.X > side / 2) { column -= side; }
    if (column - first.X < -side / 2) { column += side; }
    range.West = std::min(range.West, column);
    range.East = std::max(range.East, column);
    range.North = std::min(range.North, at.Y);
    range.South = std::max(range.South, at.Y);
  }
  const long width = range.East - range.West + 1;
  const long height = range.South - range.North + 1;
  if (width > kMaximumHeightBlocks || height > kMaximumHeightBlocks ||
      width * height > kMaximumHeightBlocks) {
    return std::unexpected("original building footprint exceeds terrain block admission");
  }
  return range;
}

std::expected<std::vector<Data::TileId>, std::string>
OriginalHeightCoverage(const Generators::RawTile &raw, int zoom, const std::stop_token &stop) {
  if (zoom < 0 || zoom > Ground::HeightField::MaximumTileZoom) {
    return std::unexpected("original buildings require a valid terrain zoom");
  }
  if (raw.Structures.empty()) { return std::vector<Data::TileId>(); }
  const auto key = [](Data::TileId tile) { return std::tuple(tile.X, tile.Y); };
  std::vector<Data::TileId> tiles;
  tiles.reserve(kMaximumHeightBlocks);
  for (const auto &building : raw.Structures) {
    const auto points = std::span(raw.LatLon)
                            .subspan(static_cast<size_t>(building.LocalFirst) * 2,
                                     static_cast<size_t>(building.PointCount) * 2);
    const auto range = FootprintTerrainRange(points, zoom, stop);
    if (!range) { return std::unexpected(range.error()); }
    for (long y = range->North; y <= range->South; ++y) {
      for (long x = range->West; x <= range->East; ++x) {
        long column = x;
        (void)Ground::WrapTile(zoom, &column, &y);
        const Data::TileId tile{
            .Zoom = zoom, .X = static_cast<uint32_t>(column), .Y = static_cast<uint32_t>(y)};
        const auto at = std::ranges::lower_bound(tiles, key(tile), {}, key);
        if (at != tiles.end() && *at == tile) { continue; }
        if (tiles.size() == static_cast<size_t>(kMaximumHeightBlocks)) {
          return std::unexpected("original building footprints exceed terrain block admission");
        }
        tiles.insert(at, tile);
      }
    }
  }
  return tiles;
}

std::expected<Generators::RawTile, std::string>
PrepareOriginal(const std::shared_ptr<const Data::OsmSourceSnapshot> &source,
                outshine::Generators::Osm::StructurePolicy policy,
                const std::stop_token &stop) {
  if (!source || source->Coverage.empty()) {
    return std::unexpected("original buildings require declared source coverage");
  }
  if (stop.stop_requested()) { return std::unexpected("original building preparation canceled"); }
  auto buildings = outshine::Generators::Osm::BuildingFootprints::Build(source, policy.PointsMost);
  if (!buildings) {
    return std::unexpected("original building footprint failed at object " +
                           std::to_string(buildings.error().Source.Id) + " with code " +
                           std::to_string(static_cast<int>(buildings.error().Code)));
  }
  Data::SourceCoverage bounds = source->Coverage.front();
  for (const auto &coverage : source->Coverage) {
    bounds.WestDeg = std::min(bounds.WestDeg, coverage.WestDeg);
    bounds.SouthDeg = std::min(bounds.SouthDeg, coverage.SouthDeg);
    bounds.EastDeg = std::max(bounds.EastDeg, coverage.EastDeg);
    bounds.NorthDeg = std::max(bounds.NorthDeg, coverage.NorthDeg);
  }
  const auto points = buildings->Points();
  for (size_t point = 0; point < points.size(); point += 2u) {
    const auto at =
        TangentFrame::At({.LongitudeDeg = points[point + 1u], .LatitudeDeg = points[point]});
    const double half = policy.PointWidthM / 2.0;
    const auto low = at.ApproximateGeographicAt({.EastM = -half, .NorthM = -half});
    const auto high = at.ApproximateGeographicAt({.EastM = half, .NorthM = half});
    bounds.WestDeg = std::min(bounds.WestDeg, low.LongitudeDeg);
    bounds.SouthDeg = std::min(bounds.SouthDeg, low.LatitudeDeg);
    bounds.EastDeg = std::max(bounds.EastDeg, high.LongitudeDeg);
    bounds.NorthDeg = std::max(bounds.NorthDeg, high.LatitudeDeg);
  }
  if (stop.stop_requested()) { return std::unexpected("original building preparation canceled"); }
  auto described =
      outshine::Generators::Osm::DescribeStructures(*buildings, source, {.Bounds = bounds}, policy);
  if (!described) {
    return std::unexpected("original building input failed with code " +
                           std::to_string(static_cast<int>(described.error())));
  }
  auto raw = Generators::StructureInput(std::move(described->Footprints));
  if (!raw) { return std::unexpected("native building footprints have an invalid cell"); }
  raw->SourceInputs.Objects = std::move(described->Source);
  raw->SourceInputs.Archive = std::move(described->Archive);
  return std::move(*raw);
}

template <typename Element, typename Access>
std::expected<void, std::string>
VerifySharedElements(std::span<const std::shared_ptr<const Data::OsmSourceSnapshot>> sources,
                     Access access,
                     const std::stop_token &stop) {
  struct Cursor {
    std::span<const Element> Elements;
    size_t At = 0;

    [[nodiscard]] const Element &Current() const { return Elements[At]; }
  };

  const auto later = [](const Cursor &a, const Cursor &b) {
    return a.Current().Id > b.Current().Id;
  };
  std::vector<Cursor> storage;
  storage.reserve(sources.size());
  for (const auto &source : sources) {
    const auto elements = access(source->Elements);
    if (!elements.empty()) { storage.push_back({.Elements = elements}); }
  }
  std::priority_queue<Cursor, std::vector<Cursor>, decltype(later)> pending(later,
                                                                            std::move(storage));
  const Element *last = nullptr;
  while (!pending.empty()) {
    if (stop.stop_requested()) { return std::unexpected("original building preparation canceled"); }
    auto next = pending.top();
    pending.pop();
    const auto &element = next.Current();
    if (last && last->Id == element.Id && *last != element) {
      return std::unexpected("original cells disagree at object " + std::to_string(element.Id));
    }
    last = &element;
    if (++next.At < next.Elements.size()) { pending.push(next); }
  }
  return {};
}

std::expected<void, std::string>
VerifySources(std::span<const std::shared_ptr<const Data::OsmSourceSnapshot>> sources,
              const std::stop_token &stop) {
  for (const auto &source : sources) {
    if (!source ||
        source->Elements.SourceIdentity() != sources.front()->Elements.SourceIdentity()) {
      return std::unexpected("original building cells require one dataset revision");
    }
  }
  if (sources.size() < 2) { return {}; }
  if (auto checked = VerifySharedElements<Data::OsmNode>(
          sources, [](const auto &elements) { return elements.Nodes(); }, stop);
      !checked) {
    return checked;
  }
  if (auto checked = VerifySharedElements<Data::OsmWay>(
          sources, [](const auto &elements) { return elements.Ways(); }, stop);
      !checked) {
    return checked;
  }
  return VerifySharedElements<Data::OsmRelation>(
      sources, [](const auto &elements) { return elements.Relations(); }, stop);
}

void RecordConsumedWays(const Generators::RawTile &raw, std::set<uint64_t> &consumedWays) {
  const auto &source =
      static_cast<const Generators::Osm::SourceCapture &>(*raw.SourceInputs.Objects);
  for (const auto &structure : raw.Structures) {
    source.RecordMemberWays(structure.SourceId, consumedWays);
  }
}

std::expected<std::vector<OriginalStructurePreparation::Product>, std::string>
PrepareProducts(std::span<const std::shared_ptr<const Data::OsmSourceSnapshot>> sources,
                outshine::Generators::Osm::StructurePolicy policy,
                int heightZoom,
                const std::stop_token &stop) {
  if (auto checked = VerifySources(sources, stop); !checked) {
    return std::unexpected(std::move(checked.error()));
  }
  std::vector<Generators::RawTile> inputs;
  inputs.reserve(sources.size());
  std::set<uint64_t> consumedWays;
  for (const auto &source : sources) {
    auto raw = PrepareOriginal(source, policy, stop);
    if (!raw) { return std::unexpected(std::move(raw.error())); }
    RecordConsumedWays(*raw, consumedWays);
    inputs.push_back(std::move(*raw));
  }
  std::set<std::pair<uint8_t, uint64_t>> owned;
  std::vector<OriginalStructurePreparation::Product> products;
  products.reserve(inputs.size());
  for (auto &raw : inputs) {
    if (stop.stop_requested()) { return std::unexpected("original building preparation canceled"); }
    std::erase_if(raw.Structures, [&](const auto &structure) {
      const auto id = structure.SourceId;
      return (id.Kind == static_cast<uint8_t>(Data::OsmElementKind::Way) &&
              consumedWays.contains(id.Id)) ||
             !owned.emplace(id.Kind, id.Id).second;
    });
    raw.SourceInputs.Origin.Selection = kDigestBasis;
    for (const auto &structure : raw.Structures) {
      raw.SourceInputs.Origin.Selection = DigestFolded(
          raw.SourceInputs.Origin.Selection, static_cast<uint8_t>(structure.SourceId.Kind));
      for (unsigned shift = 0; shift < 64u; shift += 8u) {
        raw.SourceInputs.Origin.Selection =
            DigestFolded(raw.SourceInputs.Origin.Selection,
                         static_cast<uint8_t>(structure.SourceId.Id >> shift));
      }
    }
    auto tiles = OriginalHeightCoverage(raw, heightZoom, stop);
    if (!tiles) { return std::unexpected(std::move(tiles.error())); }
    products.push_back({.Input = std::make_shared<const Generators::RawTile>(std::move(raw)),
                        .HeightTiles = std::move(*tiles)});
  }
  return products;
}

}

OriginalStructurePreparation::OriginalStructurePreparation(
    Tasks &pool,
    std::span<const std::shared_ptr<const Data::OsmSourceSnapshot>> sources,
    outshine::Generators::Osm::StructurePolicy policy,
    int heightZoom)
    : Pool_(&pool), Output_(std::make_shared<Output>()) {
  Handle_ = pool.Post([sources = std::vector(sources.begin(), sources.end()),
                       policy,
                       heightZoom,
                       output = Output_,
                       stop = Stop_.get_token()] {
    output->Value = PrepareProducts(sources, policy, heightZoom, stop);
    if (!output->Value) { return; }
    for (const auto &product : *output->Value) {
      output->HeightTiles.insert(
          output->HeightTiles.end(), product.HeightTiles.begin(), product.HeightTiles.end());
    }
    std::ranges::sort(output->HeightTiles, [](const auto &a, const auto &b) {
      return std::tie(a.Zoom, a.X, a.Y) < std::tie(b.Zoom, b.X, b.Y);
    });
    output->HeightTiles.erase(std::ranges::unique(output->HeightTiles).begin(),
                              output->HeightTiles.end());
  });
}

OriginalStructurePreparation::~OriginalStructurePreparation() {
  (void)Stop_.request_stop();
  if (Handle_ != Tasks::kNoTask) { Pool_->Wait(Handle_); }
}

OriginalStructurePreparation::Phase OriginalStructurePreparation::Poll() {
  if (Handle_ == Tasks::kNoTask || !Pool_->TakeCompletion(Handle_)) { return Phase_; }
  Handle_ = Tasks::kNoTask;
  if (Output_->Value) {
    Products_ = std::move(*Output_->Value);
    HeightTiles_ = std::move(Output_->HeightTiles);
    Phase_ = Phase::Ready;
  } else {
    Error_ = std::move(Output_->Value.error());
    Phase_ = Phase::Failed;
  }
  Output_.reset();
  return Phase_;
}

bool OriginalStructurePreparation::AwaitSlice(double seconds) const {
  return Handle_ != Tasks::kNoTask && Pool_->AwaitCompletion(seconds);
}

}

#include "PreparedOsmTiles.h"
#include "Geodesy.h"
#include "Sha256.h"
#include "TileGeodesy.h"
#include "Units.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>

namespace outshine::Generators::Osm {
namespace {
Box Bounds(const OsmField &field) {
  const auto &tile = field.Tiles().front();
  const auto range = Ground::TileBounds(
      {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)});
  double south = range.MinLatDeg;
  double north = range.MaxLatDeg;
  double west = range.MinLonDeg;
  double east = range.MaxLonDeg;
  const auto points = field.Points();
  for (size_t at = 0; at < points.size(); at += 2) {
    south = std::min(south, points[at]);
    north = std::max(north, points[at]);
    west = std::min(west, points[at + 1]);
    east = std::max(east, points[at + 1]);
  }
  Vec3 centre;
  GeoToEcef(
      {.LongitudeDeg = (west + east) * 0.5, .LatitudeDeg = (south + north) * 0.5, .HeightM = 0},
      centre);
  constexpr double derivativeM = 6400000;
  const double radius =
      derivativeM * (east - west + north - south) * std::numbers::pi / kDegPerTurn;
  Box result;
  for (size_t axis = 0; axis < 3; ++axis) {
    result.Min[axis] = centre[axis] - radius;
    result.Max[axis] = centre[axis] + radius;
  }
  return result;
}
}

std::expected<PreparedOsmTiles::Product, AssetCacheError>
PreparedOsmTiles::Load(const std::string &request, const Layout &layout, ReadCause cause) {
  auto record = Cache_->FindRequest(request);
  if (!record) { return std::unexpected(record.error()); }
  if (!*record) {
    if (cause == ReadCause::Lookup) { ++Misses_; }
    return Product{};
  }
  auto loaded = Cache_->Load((**record).Key, kPackageBytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (*loaded) {
    auto product = OsmField::DecodeNativeTile((**loaded).Bytes(), kHeapBytesMost);
    if ((**loaded).Record().Kind == "osm-tile" && product && product->Zoom_ == layout.Zoom &&
        product->Schema_ == layout.Schema && product->Layers_ == layout.Layers &&
        RequestKey({.X = product->Tiles_.front().X, .Y = product->Tiles_.front().Y}, layout) ==
            request) {
      if (cause == ReadCause::Lookup) {
        ++Hits_;
        ReadBytes_ += (**loaded).Bytes().size();
      } else {
        PublicationReadBytes_ += (**loaded).Bytes().size();
      }
      return Product(std::move(product));
    }
    const auto removed = Cache_->Remove((**record).Key);
    if (!removed) { return std::unexpected(removed.error()); }
  }
  if (cause == ReadCause::Lookup) { ++Misses_; }
  return Product{};
}

std::expected<PreparedOsmTiles::Product, std::string> PreparedOsmTiles::Generate(
    TileAt at, Data::SourceDelivery::Answer landing, const std::string &request, Work &work) {
  if (work.Cancelled.load()) { return std::unexpected("native OSM preparation cancelled"); }
  auto existing = Load(request, work.Definition, ReadCause::Publication);
  if (existing && *existing) { return std::move(*existing); }
  if (!existing && existing.error() != AssetCacheError::Busy) {
    return std::unexpected("native OSM tile cache failed");
  }
  ++Generations_;
  const auto &layout = work.Definition;
  auto field = std::make_shared<OsmField>(layout.Zoom, layout.Layers, layout.Schema);
  auto accepted = field->Accept(at.X, at.Y, landing.Bytes);
  if (!accepted) { return std::unexpected(std::string(accepted.error())); }
  auto &tile = field->Tiles_.front();
  tile.Source = {.Kind = Data::DataKind::VectorMap,
                 .Tile = {.Zoom = layout.Zoom,
                          .X = static_cast<uint32_t>(at.X),
                          .Y = static_cast<uint32_t>(at.Y)},
                 .SourceId = std::move(landing.SourceId),
                 .Revision = std::move(landing.SourceRevision)};
  auto encoded = field->EncodeNativeTile(kPackageBytesMost);
  if (!encoded) { return std::unexpected("invalid or oversized native OSM tile"); }
  const auto key =
      Sha256Hex(Recipe_ + request + tile.InputDigest + tile.Source.SourceId + tile.Source.Revision);
  const auto bounds = Bounds(*field);
  field.reset();
  if (work.Cancelled.load()) { return std::unexpected("native OSM preparation cancelled"); }
  AssetRecord record{.Key = key,
                     .Kind = "osm-tile",
                     .Bounds = bounds,
                     .Package = {},
                     .ByteCount = encoded->size(),
                     .Level = static_cast<uint32_t>(layout.Zoom),
                     .Parent = {},
                     .RequestKey = request};
  auto prepared = Cache_->PreparePublication({std::move(record)}, std::move(*encoded));
  if (!prepared) { return std::unexpected("could not prepare native OSM publication"); }
  if (work.Cancelled.load()) { return std::unexpected("native OSM preparation cancelled"); }
  SetPublication(work, std::move(*prepared));
  return Product{};
}
}

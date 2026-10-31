#include "PreparedOsmTiles.h"
#include "Geodesy.h"
#include "Sha256.h"
#include "TileGeodesy.h"
#include "Units.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <numbers>
#include <optional>
#include <ratio>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
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

std::expected<std::optional<CachedAsset>, AssetCacheError>
PreparedOsmTiles::Load(const std::string &request, size_t bytesMost) {
  auto record = Cache_->FindRequest(request);
  if (!record) { return std::unexpected(record.error()); }
  if (!*record) { return std::nullopt; }
  auto loaded = Cache_->Load((**record).Key, bytesMost);
  if (loaded && !*loaded) {
    const auto unbound = Cache_->UnbindRequestIfMatches(**record);
    if (!unbound) { return std::unexpected(unbound.error()); }
  }
  return loaded;
}

std::string PreparedOsmTiles::ProductKey(std::string_view request,
                                         std::string_view digest,
                                         std::string_view source,
                                         std::string_view revision) const {
  Sha256Prefix hash(Recipe_.data(), Recipe_.size());
  hash.Append(request.data(), request.size());
  hash.Append(digest.data(), digest.size());
  hash.Append(source.data(), source.size());
  return hash.Hex(revision.data(), revision.size());
}

std::expected<void, std::string> PreparedOsmTiles::Generate(Production &work) {
  if (!work.Landing) { return std::unexpected("native OSM generation has no source input"); }
  ++Generations_;
  const auto began = std::chrono::steady_clock::now();
  const auto &layout = work.Definition;
  auto field = std::make_unique<OsmField>(layout.Zoom, layout.Layers, layout.Schema);
  auto &landing = *work.Landing;
  auto accepted = field->Accept(work.At.X, work.At.Y, landing.Bytes);
  if (!accepted) { return std::unexpected(std::string(accepted.error())); }
  auto &tile = field->Tiles_.front();
  tile.Source = {.Kind = Data::DataKind::VectorMap,
                 .Tile = {.Zoom = layout.Zoom,
                          .X = static_cast<uint32_t>(work.At.X),
                          .Y = static_cast<uint32_t>(work.At.Y)},
                 .SourceId = std::move(landing.SourceId),
                 .Revision = std::move(landing.SourceRevision)};
  auto encoded = field->EncodeNativeTile(kPackageBytesMost);
  if (!encoded) { return std::unexpected("invalid or oversized native OSM tile"); }
  const auto bounds = Bounds(*field);
  field.reset();
  work.Landing.reset();
  AssetRecord record{.Key = work.ProductKey,
                     .Kind = "osm-tile",
                     .Bounds = bounds,
                     .Package = {},
                     .ByteCount = encoded->size(),
                     .Level = static_cast<uint32_t>(layout.Zoom),
                     .Parent = {},
                     .RequestKey = std::nullopt};
  auto prepared = Cache_->PreparePublication({std::move(record)}, std::move(*encoded));
  if (!prepared) { return std::unexpected("could not prepare native OSM publication"); }
  SetPublication(work, std::move(*prepared));
  GenerationMs_.fetch_add(
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count());
  return {};
}
}

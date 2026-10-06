#include "PreparedTerrainAssets.h"
#include "PreparedTerrainCodec.h"
#include "ByteArchive.h"
#include "SourceSet.h"
#include "Sha256.h"
#include "Geodesy.h"
#include "TileGeodesy.h"
#include <math/Units.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numbers>
#include <optional>
#include <span>
#include <system_error>
#include <utility>

namespace outshine::Generators {
namespace {

std::string Recipe(const Data::SourceSet &sources) {
  std::string recipe = "prepared-terrain-field-1";
  for (size_t at = 0; at < sources.Count(); ++at) {
    const auto &decl = sources.At(at).Declaration();
    if (decl.Kind != Data::DataKind::Elevation) { continue; }
    recipe += Data::ContentKey(decl, Data::Address::Whole(0));
    for (const auto value : {static_cast<int>(decl.Wire),
                             static_cast<int>(decl.How),
                             static_cast<int>(decl.Order),
                             static_cast<int>(decl.OnAbsent),
                             static_cast<int>(decl.TileAbsence),
                             decl.MinZoom,
                             decl.MaxZoom,
                             static_cast<int>(decl.AncestorFill)}) {
      recipe += ':';
      recipe += std::to_string(value);
    }
    recipe += ':';
    recipe += decl.Schema;
  }
  return Sha256Hex(recipe);
}

std::string Key(const std::string &recipe,
                Data::TileId at,
                const ::outshine::Ground::TerrainTiles::Shaped &shape) {
  ByteWriter encoded(4096);
  if (!encoded.Put({reinterpret_cast<const uint8_t *>(recipe.data()), recipe.size()}) ||
      !encoded.Number(at.Zoom) || !encoded.Number(at.X) || !encoded.Number(at.Y) ||
      !encoded.Number(shape.Seed)) {
    return {};
  }
  for (const auto value : {shape.AmplitudeM,
                           shape.WavelengthM,
                           shape.Gradient,
                           shape.BearingDeg,
                           shape.FocusLatDeg,
                           shape.FocusLonDeg}) {
    if (!std::isfinite(value) || !encoded.Number(value == 0.0 ? 0.0 : value)) { return {}; }
  }
  if (!encoded.Put({reinterpret_cast<const uint8_t *>(shape.Kind.data()), shape.Kind.size()})) {
    return {};
  }
  return Sha256Hex(encoded.Bytes().data(), encoded.Bytes().size());
}

Box Bounds(Data::TileId at, const ::outshine::Ground::TerrainField &field) {
  const double span = std::ldexp(1.0, at.Zoom);
  const double west = kDegPerTurn * at.X / span - kDegPerHalfTurn;
  const double east = kDegPerTurn * (at.X + 1.0) / span - kDegPerHalfTurn;
  const auto latitude = [span](double row) {
    return std::atan(std::sinh(std::numbers::pi * (1.0 - 2.0 * row / span))) * kDegPerHalfTurn /
           std::numbers::pi;
  };
  const double north = latitude(at.Y);
  const double south = latitude(at.Y + 1.0);
  const auto heights = std::span(field.Data(), static_cast<size_t>(field.Rows()) * field.Cols());
  const auto range = std::ranges::minmax_element(heights);
  Vec3 centre;
  GeoToEcef({.LongitudeDeg = (west + east) * 0.5,
             .LatitudeDeg = (south + north) * 0.5,
             .HeightM = (static_cast<double>(*range.min) + *range.max) * 0.5},
            centre);
  constexpr double earthDerivativeBoundM = 6400000.0;
  const double radius =
      (earthDerivativeBoundM + std::max(std::abs(*range.min), std::abs(*range.max))) *
          (east - west + north - south) * std::numbers::pi / kDegPerTurn +
      (static_cast<double>(*range.max) - *range.min) * 0.5;
  Box box;
  for (size_t axis = 0; axis < 3; ++axis) {
    box.Min[axis] = centre[axis] - radius;
    box.Max[axis] = centre[axis] + radius;
  }
  return box;
}

::outshine::Ground::TerrainGrid Refused(Data::TileId at, Data::FetchFailureReason reason) {
  return ::outshine::Ground::TerrainGrid::Refused(
      Data::FetchFailure{.Requested = Data::Address::At(at),
                         .Served = std::nullopt,
                         .SourceId = "prepared-terrain",
                         .SourceRevision = {},
                         .SourceKey = {},
                         .Reason = reason});
}

}

PreparedTerrainAssets::PreparedTerrainAssets(std::unique_ptr<AssetCache> cache, std::string recipe)
    : Cache_(std::move(cache)), Recipe_(std::move(recipe)) {}

std::expected<std::shared_ptr<PreparedTerrainAssets>, std::string>
PreparedTerrainAssets::Open(const std::string &directory, const Data::SourceSet &sources) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    return std::unexpected("could not create the native asset cache: " + error.message());
  }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open the native asset cache"); }
  return std::shared_ptr<PreparedTerrainAssets>(
      new PreparedTerrainAssets(std::move(*cache), Recipe(sources)));
}

::outshine::Ground::TerrainGrid
PreparedTerrainAssets::Remember(const std::string &key, ::outshine::Ground::TerrainField field) {
  auto shared = std::make_shared<const ::outshine::Ground::TerrainField>(std::move(field));
  constexpr size_t maximum = 4096;
  if (Resident_.size() >= maximum) {
    std::erase_if(Resident_, [](const auto &entry) { return entry.second.expired(); });
  }
  if (Resident_.size() < maximum) { Resident_[key] = shared; }
  return ::outshine::Ground::TerrainGrid::Holding(std::move(shared));
}

::outshine::Ground::TerrainGrid
PreparedTerrainAssets::Resolve(Data::TileId at,
                               const ::outshine::Ground::TerrainTiles::Shaped &shape,
                               const ::outshine::Ground::TerrainTiles::FieldFactory &factory) {
  using namespace ::outshine::Ground;
  if (at.Zoom < 0 || at.Zoom > Data::TileId::MaximumZoom ||
      at.X >= (uint32_t{1} << static_cast<unsigned>(at.Zoom)) ||
      at.Y >= (uint32_t{1} << static_cast<unsigned>(at.Zoom))) {
    return Refused(at, Data::FetchFailureReason::InvalidRequest);
  }
  const auto key = Key(Recipe_, at, shape);
  if (key.empty()) { return Refused(at, Data::FetchFailureReason::InvalidRequest); }
  const std::scoped_lock lock(Lock_);
  const auto held = Resident_.find(key);
  if (held != Resident_.end()) {
    if (auto field = held->second.lock()) {
      ResidentHits_.fetch_add(1, std::memory_order_relaxed);
      return TerrainGrid::Holding(std::move(field));
    }
  }
  auto loaded = Cache_->Load(key, kPreparedTerrainBytesMost);
  if (!loaded) { return Refused(at, Data::FetchFailureReason::ProviderRefused); }
  if (*loaded) {
    auto field = DecodePreparedTerrain(at, (**loaded).Bytes(), kPreparedTerrainBytesMost);
    if (field) {
      Hits_.fetch_add(1, std::memory_order_relaxed);
      ReadBytes_.fetch_add((**loaded).Bytes().size(), std::memory_order_relaxed);
      return Remember(key, std::move(*field));
    }
    if (!Cache_->Remove(key)) { return Refused(at, Data::FetchFailureReason::ProviderRefused); }
  }
  Misses_.fetch_add(1, std::memory_order_relaxed);
  if (!factory) { return Refused(at, Data::FetchFailureReason::ProviderRefused); }
  auto generated = factory();
  const auto *field = generated.TryField();
  if (field == nullptr) { return generated; }
  auto encoded = EncodePreparedTerrain(at, *field);
  if (!encoded) { return Refused(at, Data::FetchFailureReason::CorruptPayload); }
  const AssetRecord record{.Key = key,
                           .Kind = "terrain-field",
                           .Bounds = Bounds(at, *field),
                           .Package = {},
                           .ByteCount = encoded->size(),
                           .Level = static_cast<uint32_t>(at.Zoom),
                           .Parent = {}};
  if (!Cache_->Publish({&record, 1}, *encoded)) {
    return Refused(at, Data::FetchFailureReason::ProviderRefused);
  }
  Writes_.fetch_add(1, std::memory_order_relaxed);
  loaded = Cache_->Load(key, kPreparedTerrainBytesMost);
  if (!loaded || !*loaded) { return Refused(at, Data::FetchFailureReason::ProviderRefused); }
  auto ready = DecodePreparedTerrain(at, (**loaded).Bytes(), kPreparedTerrainBytesMost);
  return ready ? Remember(key, std::move(*ready))
               : Refused(at, Data::FetchFailureReason::CorruptPayload);
}

PreparedTerrainAssets::Counters PreparedTerrainAssets::Costs() const noexcept {
  return {.Hits = Hits_.load(std::memory_order_relaxed),
          .Misses = Misses_.load(std::memory_order_relaxed),
          .Resident = ResidentHits_.load(std::memory_order_relaxed),
          .Writes = Writes_.load(std::memory_order_relaxed),
          .ReadBytes = ReadBytes_.load(std::memory_order_relaxed)};
}

}

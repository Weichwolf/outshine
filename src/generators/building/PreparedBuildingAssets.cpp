#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include "AssetSourceRecipe.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include "Geodesy.h"
#include <math/Units.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <cmath>
#include <filesystem>
#include <span>
#include <system_error>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr size_t kPackageBytesMost = size_t{64} * 1024 * 1024;
constexpr size_t kResidentBytesMost = size_t{128} * 1024 * 1024;

}

Box PreparedBuildingAssets::Bounds(const PreparedStructureTile &base) {
  Box bounds;
  for (const auto &surface : base.Surfaces) {
    if (surface.Bounds().Empty()) { continue; }
    Box world = surface.Bounds();
    for (size_t axis = 0; axis < 3; ++axis) {
      world.Min[axis] += base.AnchorEcef[axis];
      world.Max[axis] += base.AnchorEcef[axis];
    }
    bounds.Cover(world.Min);
    bounds.Cover(world.Max);
  }
  for (size_t index = 0; index < base.Structures.size(); ++index) {
    if (!base.Surfaces[index].Bounds().Empty()) { continue; }
    const auto &structure = base.Structures[index];
    const auto &region = structure.Bounds;
    const double bottom = std::min(structure.Standing.FootM, structure.Standing.BaseM);
    const double top = structure.Standing.SeatM + structure.Standing.HeightM;
    Vec3 centre;
    GeoToEcef({.LongitudeDeg = (region.MinLonDeg + region.MaxLonDeg) * 0.5,
               .LatitudeDeg = (region.MinLatDeg + region.MaxLatDeg) * 0.5,
               .HeightM = (bottom + top) * 0.5},
              centre);
    constexpr double earthDerivativeBoundM = 6400000.0;
    const double radius =
        (earthDerivativeBoundM + std::max(std::abs(bottom), std::abs(top))) *
            (region.MaxLonDeg - region.MinLonDeg + region.MaxLatDeg - region.MinLatDeg) * kDeg2Rad *
            0.5 +
        std::abs(top - bottom) * 0.5;
    Box enclosure;
    for (size_t axis = 0; axis < 3; ++axis) {
      enclosure.Min[axis] = centre[axis] - radius;
      enclosure.Max[axis] = centre[axis] + radius;
    }
    bounds.Cover(enclosure);
  }
  if (bounds.Empty()) { bounds.Cover(base.AnchorEcef); }
  return bounds;
}

PreparedBuildingAssets::PreparedBuildingAssets(std::unique_ptr<AssetCache> cache,
                                               std::string recipe)
    : Cache_(std::move(cache)), Recipe_(std::move(recipe)) {}

std::expected<std::shared_ptr<PreparedBuildingAssets>, std::string>
PreparedBuildingAssets::Open(const std::string &directory, const Data::SourceSet &sources) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) { return std::unexpected("could not create the native building cache"); }
  auto cache = AssetCache::Open((std::filesystem::path(directory) / "assets.sqlite").string());
  if (!cache) { return std::unexpected("could not open the native building cache"); }
  constexpr std::array kinds{Data::DataKind::Elevation, Data::DataKind::VectorMap};
  return std::shared_ptr<PreparedBuildingAssets>(new PreparedBuildingAssets(
      std::move(*cache), AssetSourceRecipe("prepared-building-base-3", sources, kinds)));
}

std::string PreparedBuildingAssets::Key(Data::TileId tile,
                                        uint64_t streetDigest,
                                        const ::outshine::Ground::ShapedGround &shape,
                                        double spanM,
                                        std::string_view inputDigest) const {
  ByteWriter bytes(4096);
  if (!bytes.Put({reinterpret_cast<const uint8_t *>(Recipe_.data()), Recipe_.size()}) ||
      !bytes.Number(tile.Zoom) || !bytes.Number(tile.X) || !bytes.Number(tile.Y) ||
      !bytes.Number(streetDigest) || !bytes.Number(shape.Seed)) {
    return {};
  }
  for (const double value : {spanM,
                             shape.AmplitudeM,
                             shape.WavelengthM,
                             shape.Gradient,
                             shape.BearingDeg,
                             shape.FocusLatDeg,
                             shape.FocusLonDeg}) {
    if (!std::isfinite(value) || !bytes.Number(value == 0.0 ? 0.0 : value)) { return {}; }
  }
  if (!bytes.Put({reinterpret_cast<const uint8_t *>(shape.Kind.data()), shape.Kind.size()})) {
    return {};
  }
  if (!bytes.Number(inputDigest.size()) ||
      !bytes.Put({reinterpret_cast<const uint8_t *>(inputDigest.data()), inputDigest.size()})) {
    return {};
  }
  return Sha256Hex(bytes.Bytes().data(), bytes.Bytes().size());
}

std::expected<PreparedBuildingAssets::Base, StructureBakeError>
PreparedBuildingAssets::Load(const std::string &key) {
  const std::scoped_lock lock(Lock_);
  auto loaded = Cache_->Load(key, kPackageBytesMost);
  if (!loaded) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
  if (*loaded) {
    const auto bytes = (**loaded).Bytes();
    auto base = DecodePreparedStructureTile(bytes, kResidentBytesMost);
    if (base) {
      ++Hits_;
      ReadBytes_ += bytes.size();
      return std::make_shared<const PreparedStructureTile>(std::move(*base));
    }
  }
  ++Misses_;
  return Base{};
}

std::expected<PreparedBuildingAssets::Base, StructureBakeError>
PreparedBuildingAssets::Generate(const std::string &key,
                                 const RawTile &raw,
                                 const ::outshine::Ground::HeightField &heights,
                                 const std::atomic_bool &stopping) {
  auto base = PrepareStructureTile(raw, heights, &stopping);
  if (!base) { return std::unexpected(base.error()); }
  if (stopping.load(std::memory_order_relaxed)) {
    return std::unexpected(StructureBakeErrorKind::Cancelled);
  }
  auto encoded = EncodePreparedStructureTile(*base);
  if (!encoded || encoded->size() > kPackageBytesMost) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  const AssetRecord record{.Key = key,
                           .Kind = "buildings",
                           .Bounds = Bounds(*base),
                           .Package = {},
                           .ByteCount = encoded->size(),
                           .Parent = {}};
  {
    const std::scoped_lock lock(Lock_);
    const auto stored = Cache_->Publish(std::span(&record, 1), *encoded);
    if (!stored) { return std::unexpected(StructureBakeErrorKind::ArtifactFailure); }
    ++Writes_;
  }
  return Load(key);
}

PreparedBuildingAssets::Counters PreparedBuildingAssets::Costs() const noexcept {
  return {.Hits = Hits_.load(std::memory_order_relaxed),
          .Misses = Misses_.load(std::memory_order_relaxed),
          .Writes = Writes_.load(std::memory_order_relaxed),
          .ReadBytes = ReadBytes_.load(std::memory_order_relaxed),
          .GeometryHits = GeometryHits_.load(std::memory_order_relaxed),
          .GeometryMisses = GeometryMisses_.load(std::memory_order_relaxed),
          .GeometryWrites = GeometryWrites_.load(std::memory_order_relaxed),
          .GeometryReadBytes = GeometryReadBytes_.load(std::memory_order_relaxed)};
}
}

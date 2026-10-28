#include "ImpostorViewAssets.h"
#include "Material.h"
#include "Texture.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>

namespace outshine::Content {
namespace {
constexpr std::string_view kKind = "fractional-impostor-view-1";
}

bool ImpostorViewAssets::Matches(const ImpostorCards &cards, Selection selection) const {
  const uint32_t pixels = Pixels(selection);
  if (pixels == 0 || cards.Views.size() != 1 || cards.Centre != Description_.Centre ||
      cards.HalfExtentM != Description_.HalfExtentM ||
      cards.Views.front().Direction != Description_.Directions[selection.View]) {
    return false;
  }
  const auto &geometry = cards.Views.front().Surface;
  if (!geometry.wellFormed() || geometry.parts() != 1 || geometry.images() != 5 ||
      geometry.positionsOf(0).size() != 12 || geometry.trianglesOf(0).size() != 6) {
    return false;
  }
  const auto &material = geometry.surfaceAt(geometry.materialOf(0));
  if (material.Alpha != AlphaMode::Blended || material.Scattering != ScatteringModel::AreaMixture) {
    return false;
  }
  const std::array maps{material.BaseColourMap.Image,
                        material.NormalMap.Image,
                        material.MetalRoughMap.Image,
                        material.DiffuseReflectionMap.Image,
                        material.DiffuseTransmissionMap.Image};
  for (size_t at = 0; at < maps.size(); ++at) {
    const auto image = geometry.imageAt(maps[at]);
    const auto format = at == 2 ? ImagePixelFormat::Rgba16Float : ImagePixelFormat::Rgba8Unorm;
    if (std::cmp_not_equal(image.WidthPx, pixels) || std::cmp_not_equal(image.HeightPx, pixels) ||
        image.Format != format) {
      return false;
    }
  }
  return true;
}

std::expected<void, AssetCacheError>
ImpostorViewAssets::Publish(Selection selection, Geometry geometry, size_t bytesMost) const {
  if (Pixels(selection) == 0) { return std::unexpected(AssetCacheError::InvalidInput); }
  ImpostorCards cards{
      .Centre = Description_.Centre, .HalfExtentM = Description_.HalfExtentM, .Views = {}};
  cards.Views.push_back(
      {.Direction = Description_.Directions[selection.View], .Surface = std::move(geometry)});
  if (!Matches(cards, selection)) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto key = Key(selection);
  const auto bytes = cards.Encode(key, bytesMost);
  if (!bytes) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  const AssetRecord record{.Key = key,
                           .Kind = std::string(kKind),
                           .Bounds = Record_.Bounds,
                           .Package = {},
                           .OffsetBytes = 0,
                           .ByteCount = bytes->size(),
                           .Level = selection.Mip,
                           .Parent = Record_.Key,
                           .RequestKey = {}};
  return Cache_.Publish(std::span(&record, 1), *bytes);
}

std::expected<std::optional<ImpostorCards>, AssetCacheError>
ImpostorViewAssets::Load(Selection selection, size_t bytesMost) const {
  if (Pixels(selection) == 0) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto key = Key(selection);
  const auto asset = Cache_.Load(key, bytesMost);
  if (!asset) { return std::unexpected(asset.error()); }
  if (!*asset) { return std::nullopt; }
  const auto &record = (**asset).Record();
  if (record.Kind != kKind || record.Parent != Record_.Key || record.Level != selection.Mip ||
      record.Bounds != Record_.Bounds || record.OffsetBytes != 0) {
    return std::nullopt;
  }
  auto cards = ImpostorCards::Decode((**asset).Bytes(), key, bytesMost);
  if (!cards || !Matches(*cards, selection)) { return std::nullopt; }
  return cards;
}

std::expected<ImpostorCards, AssetCacheError>
ImpostorViewAssets::Resolve(Selection selection, size_t bytesMost, const Factory &factory) const {
  if (bytesMost == 0) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  auto loaded = Load(selection, bytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (*loaded) { return std::move(**loaded); }
  if (!factory) { return std::unexpected(AssetCacheError::InvalidInput); }
  auto generated = factory(selection, bytesMost);
  if (!generated) { return std::unexpected(generated.error()); }
  const auto published = Publish(selection, std::move(*generated), bytesMost);
  if (!published) { return std::unexpected(published.error()); }
  loaded = Load(selection, bytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return std::unexpected(AssetCacheError::Storage); }
  return std::move(**loaded);
}
}

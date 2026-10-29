#include "ImpostorViewAssets.h"
#include "Sha256.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Content {
namespace {
constexpr std::string_view kFieldKind = "impostor-material-view-1";
}

std::string ImpostorViewAssets::FieldKey(ViewIndex view) const {
  return Sha256Hex(std::string(kFieldKind) + "/" + Record_.Package + "/" +
                   std::to_string(view.Value));
}

bool ImpostorViewAssets::MatchesField(const ImpostorAtlas &field, ViewIndex view) const noexcept {
  return view.Value < Description_.Directions.size() && field.Views().size() == 1 &&
         field.CentreM() == Description_.Centre &&
         field.HalfExtentM() == Description_.HalfExtentM &&
         std::cmp_equal(field.Pixels(), Description_.Pixels) &&
         field.Views().front().TowardEye == Description_.Directions[view.Value];
}

std::expected<void, AssetCacheError> ImpostorViewAssets::PublishField(ViewIndex view,
                                                                      const ImpostorAtlas &field,
                                                                      size_t bytesMost) const {
  auto prepared = PrepareField(view, field, bytesMost);
  if (!prepared) { return std::unexpected(prepared.error()); }
  const auto published = Publish(*prepared);
  if (!published) { return std::unexpected(published.error()); }
  return {};
}

std::expected<ImpostorViewAssets::ProductWrite, AssetCacheError> ImpostorViewAssets::PrepareField(
    ViewIndex view, const ImpostorAtlas &field, size_t bytesMost) const {
  auto encoded = EncodeField(view, field, bytesMost);
  if (!encoded) { return std::unexpected(encoded.error()); }
  return Prepare(std::move(*encoded));
}

std::expected<ImpostorViewAssets::EncodedProduct, AssetCacheError> ImpostorViewAssets::EncodeField(
    ViewIndex view, const ImpostorAtlas &field, size_t bytesMost) const {
  if (!MatchesField(field, view)) { return std::unexpected(AssetCacheError::InvalidInput); }
  std::string error;
  const auto key = FieldKey(view);
  auto bytes = field.Encode(key, bytesMost, error);
  if (!bytes) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  AssetRecord record{.Key = key,
                     .Kind = std::string(kFieldKind),
                     .Bounds = Record_.Bounds,
                     .Package = {},
                     .OffsetBytes = 0,
                     .ByteCount = bytes->size(),
                     .Level = 0,
                     .Parent = Record_.Key,
                     .RequestKey = {}};
  return EncodedProduct{.Record = std::move(record), .Bytes = std::move(*bytes)};
}

std::expected<std::vector<uint8_t>, AssetCacheError>
ImpostorViewAssets::StoreField(ViewIndex view, const ImpostorAtlas &field, size_t bytesMost) const {
  auto encoded = EncodeField(view, field, bytesMost);
  if (!encoded) { return std::unexpected(encoded.error()); }
  const auto published = Cache_.Publish(std::span(&encoded->Record, 1), encoded->Bytes);
  if (!published) { return std::unexpected(published.error()); }
  return std::move(encoded->Bytes);
}

std::expected<std::optional<ImpostorAtlas>, AssetCacheError>
ImpostorViewAssets::LoadField(ViewIndex view, size_t bytesMost) const {
  if (view.Value >= Description_.Directions.size()) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  const auto key = FieldKey(view);
  const auto asset = Cache_.Load(key, bytesMost);
  if (!asset) { return std::unexpected(asset.error()); }
  if (!*asset) { return std::nullopt; }
  const auto &record = (**asset).Record();
  if (record.Kind != kFieldKind || record.Parent != Record_.Key || record.Level != 0 ||
      record.Bounds != Record_.Bounds || record.OffsetBytes != 0) {
    return std::nullopt;
  }
  std::string error;
  auto field = ImpostorAtlas::Decode((**asset).Bytes(), key, error);
  if (!field || !MatchesField(*field, view)) { return std::nullopt; }
  return field;
}

std::expected<ImpostorAtlas, AssetCacheError> ImpostorViewAssets::ResolveField(
    ViewIndex view, size_t bytesMost, const FieldFactory &factory) const {
  if (bytesMost == 0) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  auto loaded = LoadField(view, bytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (*loaded) { return std::move(**loaded); }
  if (!factory) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto encoded = [&] -> std::expected<std::vector<uint8_t>, AssetCacheError> {
    auto generated = factory(view, bytesMost);
    if (!generated) { return std::unexpected(generated.error()); }
    return StoreField(view, *generated, bytesMost);
  }();
  if (!encoded) { return std::unexpected(encoded.error()); }
  std::string error;
  auto field = ImpostorAtlas::Decode(*encoded, FieldKey(view), error);
  if (!field) { return std::unexpected(AssetCacheError::Storage); }
  return std::move(*field);
}
}

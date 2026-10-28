#include "ImpostorViewAssets.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <cmath>
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
constexpr uint64_t kFormat = 0x000131565046ULL;
constexpr size_t kMetadataBytesMost = 4096;
constexpr double kDirectionNormTolerance = 1e-9;
constexpr std::string_view kKind = "fractional-impostor-views-1";

std::string IndexKey(std::string_view provenance) {
  return Sha256Hex(std::string(kKind) + "/" + std::string(provenance));
}

bool Valid(const ImpostorViewAssets::Description &description) {
  return description.Pixels >= 3 && description.Pixels <= 4096 && description.HalfExtentM > 0 &&
         std::isfinite(description.HalfExtentM) &&
         std::ranges::all_of(description.Centre,
                             [](double value) { return std::isfinite(value); }) &&
         !description.Directions.empty() && description.Directions.size() <= 64 &&
         std::ranges::all_of(description.Directions, [](const Vec3 &direction) {
           const double norm = Dot(direction, direction);
           return std::isfinite(norm) && std::abs(norm - 1) <= kDirectionNormTolerance;
         });
}

std::optional<std::vector<uint8_t>> Encode(const ImpostorViewAssets::Description &description,
                                           std::string_view provenance,
                                           const Box &bounds) {
  if (provenance.empty() || !Valid(description)) { return std::nullopt; }
  BinaryValueWriter out(kMetadataBytesMost);
  if (!out(kFormat) || !out.Text(provenance) || !out(bounds.Min.Axis, bounds.Max.Axis) ||
      !out(description.Centre.Axis,
           description.HalfExtentM,
           description.Pixels,
           static_cast<uint32_t>(description.Directions.size()))) {
    return std::nullopt;
  }
  for (const auto &direction : description.Directions) {
    if (!out(direction.Axis)) { return std::nullopt; }
  }
  return std::move(out.Out).TakeBytes();
}

std::optional<ImpostorViewAssets::Description>
Decode(std::span<const uint8_t> bytes, std::string_view provenance, const Box &bounds) {
  BinaryValueReader in(bytes);
  ImpostorViewAssets::Description description;
  uint64_t format = 0;
  uint32_t count = 0;
  std::string source;
  Box stored;
  if (!in(format) || format != kFormat || !in.Text(source) || source != provenance ||
      !in(stored.Min.Axis, stored.Max.Axis) || stored != bounds ||
      !in(description.Centre.Axis, description.HalfExtentM, description.Pixels, count) ||
      count == 0 || count > 64 || count > in.In.Remaining() / (3 * sizeof(double))) {
    return std::nullopt;
  }
  description.Directions.resize(count);
  for (auto &direction : description.Directions) {
    if (!in(direction.Axis)) { return std::nullopt; }
  }
  if (in.In.Remaining() != 0 || !Valid(description)) { return std::nullopt; }
  return description;
}
}

ImpostorViewAssets::ImpostorViewAssets(AssetCache &cache,
                                       AssetRecord record,
                                       Description description)
    : Cache_(cache), Record_(std::move(record)), Description_(std::move(description)) {}

std::expected<ImpostorViewAssets, AssetCacheError> ImpostorViewAssets::Publish(
    AssetCache &cache, std::string_view provenance, Description description, Box bounds) {
  const auto bytes = Encode(description, provenance, bounds);
  if (!bytes) { return std::unexpected(AssetCacheError::InvalidInput); }
  AssetRecord record{.Key = IndexKey(provenance),
                     .Kind = std::string(kKind),
                     .Bounds = bounds,
                     .Package = Sha256Hex(bytes->data(), bytes->size()),
                     .OffsetBytes = 0,
                     .ByteCount = bytes->size(),
                     .Level = 0,
                     .Parent = {},
                     .RequestKey = {}};
  const auto published = cache.Publish(std::span(&record, 1), *bytes);
  if (!published) { return std::unexpected(published.error()); }
  return ImpostorViewAssets(cache, std::move(record), std::move(description));
}

std::expected<std::optional<ImpostorViewAssets>, AssetCacheError>
ImpostorViewAssets::Open(AssetCache &cache, std::string_view provenance) {
  if (provenance.empty()) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto asset = cache.Load(IndexKey(provenance), kMetadataBytesMost);
  if (!asset) { return std::unexpected(asset.error()); }
  if (!*asset || (**asset).Record().Kind != kKind) { return std::nullopt; }
  auto description = Decode((**asset).Bytes(), provenance, (**asset).Record().Bounds);
  if (!description) { return std::nullopt; }
  return ImpostorViewAssets(cache, (**asset).Record(), std::move(*description));
}

uint32_t ImpostorViewAssets::Pixels(Selection selection) const noexcept {
  if (selection.View >= Description_.Directions.size()) { return 0; }
  uint32_t pixels = Description_.Pixels;
  for (uint32_t mip = 0; mip < selection.Mip; ++mip) {
    if (pixels <= 1) { return 0; }
    pixels /= 2;
  }
  return pixels;
}

std::optional<ImpostorViewAssets::Selection>
ImpostorViewAssets::Select(Vec3 towardEye, double diameterPx) const noexcept {
  if (!std::isfinite(diameterPx) || diameterPx <= 0 || !Normalise(towardEye)) {
    return std::nullopt;
  }
  Selection selected;
  double score = Dot(towardEye, Description_.Directions.front());
  for (size_t at = 1; at < Description_.Directions.size(); ++at) {
    const double next = Dot(towardEye, Description_.Directions[at]);
    if (next > score) {
      score = next;
      selected.View = at;
    }
  }
  for (uint32_t pixels = Description_.Pixels; pixels > 1;) {
    const uint32_t next = pixels / 2;
    if (static_cast<double>(next) < diameterPx) { break; }
    ++selected.Mip;
    pixels = next;
  }
  return selected;
}

std::string ImpostorViewAssets::Key(Selection selection) const {
  return Sha256Hex(std::string(kKind) + "/" + Record_.Package + "/" +
                   std::to_string(selection.View) + "/" + std::to_string(selection.Mip));
}
}

#ifndef OUTSHINE_CONTENT_ASSETS_ASSETCACHE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETCACHE_H

#include "Box.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

struct AssetRecord {
  std::string Key;
  std::string Kind;
  Box Bounds;
  std::string Package;
  uint64_t OffsetBytes = 0;
  uint64_t ByteCount = 0;
  uint32_t Level = 0;
  std::string Parent;

  [[nodiscard]] bool operator==(const AssetRecord &) const = default;
};

struct AssetRadius {
  Vec3 Centre;
  double Metres = 0;
};

struct AssetQuery {
  Box Bounds;
  std::string_view Kind;
  std::optional<uint32_t> Level;
  std::optional<AssetRadius> Radius;
  std::span<const std::array<double, 4>> Planes;
};

enum class AssetCacheError { InvalidInput, Storage, Busy, UnsupportedVersion, CapacityExceeded };

class CachedAsset {
public:
  [[nodiscard]] const AssetRecord &Record() const noexcept { return Record_; }

  [[nodiscard]] std::span<const uint8_t> Bytes() const noexcept;

private:
  friend class AssetCache;
  CachedAsset(AssetRecord record, std::shared_ptr<const std::vector<uint8_t>> bytes);
  AssetRecord Record_;
  std::shared_ptr<const std::vector<uint8_t>> Package_;
};

class AssetCache {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<AssetCache>, AssetCacheError>
  Open(const std::string &path);
  ~AssetCache();
  AssetCache(const AssetCache &) = delete;
  AssetCache &operator=(const AssetCache &) = delete;

  [[nodiscard]] std::expected<void, AssetCacheError> Publish(std::span<const AssetRecord> records,
                                                             std::span<const uint8_t> payload);
  [[nodiscard]] std::expected<std::optional<AssetRecord>, AssetCacheError>
  Find(std::string_view key) const;
  [[nodiscard]] std::expected<std::optional<CachedAsset>, AssetCacheError>
  Load(std::string_view key, size_t packageBytesMost) const;
  [[nodiscard]] std::expected<std::vector<AssetRecord>, AssetCacheError>
  Select(const AssetQuery &query) const;
  [[nodiscard]] std::expected<void, AssetCacheError> Remove(std::string_view key);

private:
  struct State;
  explicit AssetCache(std::unique_ptr<State> state);
  std::unique_ptr<State> State_;
};

}
#endif

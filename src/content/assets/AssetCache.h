#ifndef OUTSHINE_CONTENT_ASSETS_ASSETCACHE_H
#define OUTSHINE_CONTENT_ASSETS_ASSETCACHE_H

#include "Box.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <functional>
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
  std::optional<std::string> RequestKey = std::nullopt;

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

class PreparedAssetWrite {
public:
  PreparedAssetWrite(PreparedAssetWrite &&) noexcept = default;
  PreparedAssetWrite &operator=(PreparedAssetWrite &&) noexcept = default;
  PreparedAssetWrite(const PreparedAssetWrite &) = delete;
  PreparedAssetWrite &operator=(const PreparedAssetWrite &) = delete;

  [[nodiscard]] size_t NativeBytes() const noexcept { return NativeBytes_; }

  [[nodiscard]] size_t StoredBytes() const noexcept { return Bytes_.size(); }

  // C++ object/buffer capacities, including inline strings; excludes allocator overhead.
  [[nodiscard]] size_t CapacityBytes() const noexcept;

private:
  friend class AssetCache;
  PreparedAssetWrite() = default;
  std::vector<AssetRecord> Records_;
  std::vector<uint8_t> Bytes_;
  std::string Key_;
  size_t NativeBytes_ = 0;
  bool Compressed_ = false;
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
  [[nodiscard]] std::expected<PreparedAssetWrite, AssetCacheError>
  PreparePublication(std::vector<AssetRecord> records, std::vector<uint8_t> payload);
  // Busy leaves the owned package intact for a later background step.
  [[nodiscard]] std::expected<void, AssetCacheError> Publish(const PreparedAssetWrite &prepared);

  [[nodiscard]] std::expected<std::optional<AssetRecord>, AssetCacheError>
  Find(std::string_view key) const;

  [[nodiscard]] std::expected<std::optional<AssetRecord>, AssetCacheError>
  FindRequest(std::string_view requestKey) const;

  [[nodiscard]] std::expected<bool, AssetCacheError> BindRequest(std::string_view key,
                                                                 std::string_view requestKey);

  [[nodiscard]] std::expected<std::optional<CachedAsset>, AssetCacheError>
  Load(std::string_view key, size_t packageBytesMost) const;
  using Validator = std::function<std::expected<bool, AssetCacheError>(const CachedAsset &)>;
  // Normal hits validate without a writer. Invalid entries are reread and validated
  // under one write transaction before removal. Validators may do bounded native
  // validation and same-store reads; no writes, source IO or blocking waits.
  [[nodiscard]] std::expected<std::optional<CachedAsset>, AssetCacheError>
  LoadValidated(std::string_view key,
                std::string_view kind,
                size_t packageBytesMost,
                const Validator &validate);
  [[nodiscard]] std::expected<std::optional<CachedAsset>, AssetCacheError>
  LoadRequest(std::string_view requestKey, size_t packageBytesMost) const;
  void UsePackage(std::string_view packageKey) const;

  [[nodiscard]] std::expected<std::vector<AssetRecord>, AssetCacheError>
  Select(const AssetQuery &query) const;

  [[nodiscard]] std::expected<void, AssetCacheError> Remove(std::string_view key);

private:
  struct State;
  struct Publication;
  [[nodiscard]] std::expected<void, AssetCacheError>
  PublishPrepared(const Publication &publication);
  explicit AssetCache(std::unique_ptr<State> state);
  std::unique_ptr<State> State_;
};

}
#endif

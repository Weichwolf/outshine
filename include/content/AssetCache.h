#ifndef OUTSHINE_CONTENT_ASSETCACHE_H
#define OUTSHINE_CONTENT_ASSETCACHE_H

#include "math/Box.h"
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

/// Persistent native product metadata. Keys bind the generator's version, inputs and parameters.
/// All nonempty keys use 64 lowercase hexadecimal SHA-256 characters. Bounds are finite,
/// ordered and float-representable; one cache uses a common metric world frame, normally ECEF.
struct AssetRecord {
  std::string Key;     ///< Stable product identity.
  std::string Kind;    ///< Nonempty product kind owned by its generator or decoder.
  Box Bounds;          ///< Conservative closed bounds in the common world frame, in metres.
  std::string Package; ///< Payload identity; empty on Publish derives it, otherwise it must match.
  uint64_t OffsetBytes = 0; ///< Beginning of this product's slice within the package.
  uint64_t ByteCount = 0;   ///< Length of this product's slice, including its format metadata.
  uint32_t Level = 0;       ///< Generator-defined hierarchy or detail level.
  std::string Parent;       ///< Parent product identity, or empty for a root.

  /// @return Exact metadata equality; does not load or compare package bytes.
  [[nodiscard]] bool operator==(const AssetRecord &) const = default;
};

/// Closed sphere query in the cache's world frame.
struct AssetRadius {
  Vec3 Centre;       ///< Sphere centre, in metres.
  double Metres = 0; ///< Finite nonnegative radius, in metres.
};

/// Intersect the envelope with optional kind, level, sphere and frustum filters.
/// Views are borrowed only during Select; planes keep dot(normal, point) + offset >= 0.
struct AssetQuery {
  Box Bounds;                        ///< Finite, ordered query envelope in the cache's world frame.
  std::string_view Kind;             ///< Empty matches all kinds.
  std::optional<uint32_t> Level;     ///< Empty matches all levels.
  std::optional<AssetRadius> Radius; ///< Empty omits the sphere test.
  std::span<const std::array<double, 4>> Planes; ///< Finite inward-facing plane coefficients.
};

/// Cache failures; a missing or corrupt stored package is an ordinary load miss.
enum class AssetCacheError {
  InvalidInput,       ///< Invalid key, bounds, slice or query.
  Storage,            ///< File or database operation failed.
  Busy,               ///< Another connection holds the required database boundary.
  UnsupportedVersion, ///< Database schema cannot be read by this implementation.
  CapacityExceeded    ///< Whole package exceeds the caller's byte budget.
};

/// Immutable package lease. Copies share storage; bytes survive cache closure and record removal.
/// A returned span borrows a live lease; do not access a moved-from lease.
class CachedAsset {
public:
  /// @return Borrowed slice metadata for the lease's lifetime.
  [[nodiscard]] const AssetRecord &Record() const noexcept { return Record_; }

  /// @return Borrowed product slice, excluding other products in the same package.
  [[nodiscard]] std::span<const uint8_t> Bytes() const noexcept;

private:
  friend class AssetCache;
  CachedAsset(AssetRecord record, std::shared_ptr<const std::vector<uint8_t>> bytes);
  AssetRecord Record_;
  std::shared_ptr<const std::vector<uint8_t>> Package_;
};

/// Persistent native packages and their spatial index; product formats remain caller-owned.
/// Calls are synchronous and may perform disk IO. Serialize access to each instance externally;
/// independent instances may use the same database and report Busy without partial publication.
class AssetCache {
public:
  /// Open or create a database file; its parent directory must exist. Allocates the owned handle.
  /// @param path Database filename in the host filesystem's encoding.
  /// @return Owned cache instance or a storage/schema error.
  [[nodiscard]] static std::expected<std::unique_ptr<AssetCache>, AssetCacheError>
  Open(const std::string &path);
  ~AssetCache(); ///< Close the database; outstanding package leases retain their bytes.
  AssetCache(const AssetCache &) = delete;
  AssetCache &operator=(const AssetCache &) = delete;

  /// Atomically publish all slices and the immutable payload. Validate before changing the index.
  /// Empty Package fields derive the payload hash; supplied ones must match. Slices lie within
  /// payload.
  /// @param records Nonempty borrowed metadata, with distinct valid keys and in-bounds slices.
  /// @param payload Borrowed package bytes; neither argument is retained beyond the call.
  /// @return Success or an error without partial publication.
  [[nodiscard]] std::expected<void, AssetCacheError> Publish(std::span<const AssetRecord> records,
                                                             std::span<const uint8_t> payload);
  /// Read metadata without reading the payload; empty means no valid indexed record.
  /// @param key Product identity; 64 lowercase hexadecimal characters.
  /// @return Indexed metadata, a miss or an error.
  [[nodiscard]] std::expected<std::optional<AssetRecord>, AssetCacheError>
  Find(std::string_view key) const;
  /// Load and validate the whole package within packageBytesMost; corrupt or absent data is a miss.
  /// @param key Product identity; 64 lowercase hexadecimal characters.
  /// @param packageBytesMost Maximum whole-package allocation in bytes, not just this slice.
  /// @return Immutable owned lease, a miss or an error before publication to the caller.
  [[nodiscard]] std::expected<std::optional<CachedAsset>, AssetCacheError>
  Load(std::string_view key, size_t packageBytesMost) const;
  /// Return spatially filtered metadata without decoding products or reading their payloads.
  /// @param query Borrowed finite filters in the cache's common metric world frame.
  /// @return Owned matching metadata or an error; result order is unspecified.
  [[nodiscard]] std::expected<std::vector<AssetRecord>, AssetCacheError>
  Select(const AssetQuery &query) const;
  /// Remove one indexed product; existing leases and immutable package bytes remain valid.
  /// @param key Product identity; 64 lowercase hexadecimal characters.
  /// @return Success, including an already absent key, or an error.
  [[nodiscard]] std::expected<void, AssetCacheError> Remove(std::string_view key);

private:
  struct State;
  explicit AssetCache(std::unique_ptr<State> state);
  std::unique_ptr<State> State_;
};

}
#endif

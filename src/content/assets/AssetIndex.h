#ifndef OUTSHINE_CONTENT_ASSETS_ASSETINDEX_H
#define OUTSHINE_CONTENT_ASSETS_ASSETINDEX_H

#include "Box.h"
#include <array>
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

enum class AssetIndexError { InvalidInput, Storage, Busy, UnsupportedVersion };

class AssetIndex {
public:
  [[nodiscard]] static std::expected<std::unique_ptr<AssetIndex>, AssetIndexError>
  Open(const std::string &path);
  ~AssetIndex();
  AssetIndex(const AssetIndex &) = delete;
  AssetIndex &operator=(const AssetIndex &) = delete;

  [[nodiscard]] std::expected<void, AssetIndexError> Publish(std::span<const AssetRecord> records);
  [[nodiscard]] std::expected<std::optional<AssetRecord>, AssetIndexError>
  Find(std::string_view key) const;
  [[nodiscard]] std::expected<std::vector<AssetRecord>, AssetIndexError>
  Select(const AssetQuery &query) const;
  [[nodiscard]] std::expected<void, AssetIndexError> Remove(std::string_view key);

private:
  struct State;
  explicit AssetIndex(std::unique_ptr<State> state);
  std::unique_ptr<State> State_;
};

}
#endif

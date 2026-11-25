#ifndef OUTSHINE_CONTENT_ASSETS_ASSETRECORD_H
#define OUTSHINE_CONTENT_ASSETS_ASSETRECORD_H

#include "Box.h"
#include <cstdint>
#include <optional>
#include <string>

namespace outshine {
struct AssetTile {
  std::string Revision;
  std::string Coverage;
  std::string Coarser;
  // Unknown error remains eligible as fallback coverage.
  std::optional<double> ErrorM = std::nullopt;

  [[nodiscard]] bool operator==(const AssetTile &) const = default;
};

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
  std::optional<AssetTile> Tile = std::nullopt;

  [[nodiscard]] bool operator==(const AssetRecord &) const = default;
};

}
#endif

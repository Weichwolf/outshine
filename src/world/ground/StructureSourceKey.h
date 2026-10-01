#ifndef OUTSHINE_WORLD_GROUND_STRUCTURESOURCEKEY_H
#define OUTSHINE_WORLD_GROUND_STRUCTURESOURCEKEY_H

#include "Digest.h"
#include "TileSourceIdentity.h"
#include "OsmSourceSnapshot.h"

#include <memory>

#include <bit>
#include <cstdint>
#include <optional>
#include <algorithm>
#include <span>
#include <string_view>
#include <vector>

namespace outshine {

struct StructureOriginalSource {
  std::shared_ptr<const Data::OsmSourceSnapshot> Snapshot;
  Data::SourceCoverage Bounds;
};

struct StructureSourceView {
  const std::optional<Data::TileSourceIdentity> &Vector;
  std::span<const Data::TileSourceIdentity> HeightSources;
  uint64_t HeightDigest = 0;
  uint64_t StreetDigest = 0;
  double TileSpanM = 0;
  bool FallbackHeights = false;
  const StructureOriginalSource *Original = nullptr;
};

[[nodiscard]] inline uint64_t StructureSourceKey(const StructureSourceView &inputs) {
  uint64_t digest = kDigestBasis;
  const auto word = [&digest](uint64_t value) {
    for (unsigned shift = 0; shift < 64u; shift += 8u) {
      digest = DigestFolded(digest, static_cast<uint8_t>(value >> shift));
    }
  };
  const auto bytes = [&digest, &word](std::string_view value) {
    word(value.size());
    for (const char byte : value) { digest = DigestFolded(digest, static_cast<uint8_t>(byte)); }
  };
  const auto source = [&word, &bytes](const Data::TileSourceIdentity &value) {
    word(static_cast<uint64_t>(value.From));
    word(static_cast<uint64_t>(value.Kind));
    word(static_cast<uint64_t>(value.Tile.Zoom));
    word(value.Tile.X);
    word(value.Tile.Y);
    if (value.NativeCell) {
      bytes("native-cell");
      word(static_cast<uint32_t>(value.NativeCell->SouthDeg));
      word(static_cast<uint32_t>(value.NativeCell->WestDeg));
    }
    bytes(value.SourceId);
    bytes(value.Revision);
  };
  word(static_cast<uint64_t>(inputs.Vector.has_value()));
  if (inputs.Vector) { source(*inputs.Vector); }
  if (inputs.Original != nullptr && inputs.Original->Snapshot) {
    bytes("original-osm");
    const auto &identity = inputs.Original->Snapshot->Elements.SourceIdentity();
    bytes(identity.DatasetId);
    bytes(identity.Revision);
    std::vector<std::string_view> payloads;
    payloads.reserve(inputs.Original->Snapshot->Chunks.size());
    for (const auto &chunk : inputs.Original->Snapshot->Chunks) {
      payloads.push_back(chunk.PayloadSha256);
    }
    std::ranges::sort(payloads);
    payloads.erase(std::ranges::unique(payloads).begin(), payloads.end());
    word(payloads.size());
    for (const auto payload : payloads) { bytes(payload); }
    const auto &bounds = inputs.Original->Bounds;
    word(std::bit_cast<uint64_t>(bounds.WestDeg));
    word(std::bit_cast<uint64_t>(bounds.SouthDeg));
    word(std::bit_cast<uint64_t>(bounds.EastDeg));
    word(std::bit_cast<uint64_t>(bounds.NorthDeg));
  }
  std::span<const Data::TileSourceIdentity> heights = inputs.HeightSources;
  std::vector<Data::TileSourceIdentity> normalized;
  if (!std::ranges::is_sorted(heights) || std::ranges::adjacent_find(heights) != heights.end()) {
    normalized.assign(heights.begin(), heights.end());
    std::ranges::sort(normalized);
    normalized.erase(std::ranges::unique(normalized).begin(), normalized.end());
    heights = normalized;
  }
  word(heights.size());
  for (const Data::TileSourceIdentity &height : heights) { source(height); }
  word(inputs.HeightDigest);
  word(inputs.StreetDigest);
  word(std::bit_cast<uint64_t>(inputs.TileSpanM));
  word(static_cast<uint64_t>(inputs.FallbackHeights));
  return digest == 0 ? kDigestBasis : digest;
}

}

#endif

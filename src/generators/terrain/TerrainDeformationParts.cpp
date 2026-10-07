#include "TerrainDeformationParts.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <utility>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint32_t kFormat = 0x314d4454;
constexpr size_t kPagesMost = 65536;
constexpr size_t kHeaderMost = 512;
}

size_t TerrainDeformationHeap(std::span<const Sheet> pages) noexcept {
  size_t bytes = pages.size() * sizeof(Sheet);
  for (const Sheet &page : pages) { bytes += page.Nodes.size() * sizeof(float); }
  return bytes;
}

std::optional<std::vector<uint32_t>> TerrainDeformationPageCounts(std::span<const Sheet> pages) {
  if (pages.size() > kPagesMost) { return std::nullopt; }
  std::vector<uint32_t> counts;
  size_t bytes = kHeaderMost;
  uint32_t count = 0;
  for (const Sheet &page : pages) {
    if (page.Nodes.size() >
        (kTerrainDeformationBytesMost - kHeaderMost - sizeof(Sheet)) / sizeof(float)) {
      return std::nullopt;
    }
    const size_t held = sizeof(Sheet) + page.Nodes.size() * sizeof(float);
    if (held > kTerrainDeformationBytesMost - bytes) {
      counts.push_back(count);
      count = 0;
      bytes = kHeaderMost;
    }
    bytes += held;
    ++count;
  }
  counts.push_back(count);
  return counts;
}

std::string TerrainDeformationPartKey(const std::string &key, size_t part) {
  const std::string value = "terrain-deformed-part:" + key + ":" + std::to_string(part);
  return Sha256Hex(value.data(), value.size());
}

std::optional<std::vector<uint8_t>>
EncodeTerrainDeformationParts(const std::string &key, const TerrainDeformationParts &parts) {
  auto metadata = EncodeTerrainDeformation(key, {}, parts.Effects);
  if (!metadata || metadata->size() > kHeaderMost || parts.PageCounts.size() < 2 ||
      parts.PageCounts.size() > kPagesMost) {
    return std::nullopt;
  }
  ByteWriter out(kTerrainDeformationBytesMost);
  if (!out.Number(kFormat) || !out.Number(parts.HeapBytes) ||
      !out.Number(static_cast<uint32_t>(metadata->size())) || !out.Put(*metadata) ||
      !out.Number(static_cast<uint32_t>(parts.PageCounts.size()))) {
    return std::nullopt;
  }
  size_t total = 0;
  for (const uint32_t count : parts.PageCounts) {
    total += count;
    if (count == 0 || total > kPagesMost || !out.Number(count)) { return std::nullopt; }
  }
  return std::move(out).TakeBytes();
}

std::optional<TerrainDeformationParts>
DecodeTerrainDeformationParts(const std::string &key, std::span<const uint8_t> bytes) {
  ByteReader in(bytes);
  uint32_t format = 0;
  uint32_t metadataBytes = 0;
  uint32_t count = 0;
  TerrainDeformationParts parts;
  if (!in.Number(format) || format != kFormat || !in.Number(parts.HeapBytes) ||
      !in.Number(metadataBytes) || metadataBytes > kHeaderMost) {
    return std::nullopt;
  }
  const auto metadata = in.Take(metadataBytes);
  if (!metadata) { return std::nullopt; }
  auto decoded = DecodeTerrainDeformation(key, *metadata);
  if (!decoded || !decoded->Pages.empty() || !in.Number(count) || count < 2 || count > kPagesMost ||
      count > in.Remaining() / sizeof(uint32_t)) {
    return std::nullopt;
  }
  parts.Effects = decoded->Effects;
  parts.PageCounts.resize(count);
  size_t total = 0;
  for (uint32_t &pages : parts.PageCounts) {
    if (!in.Number(pages) || pages == 0 || pages > kPagesMost - total) { return std::nullopt; }
    total += pages;
  }
  if (in.Remaining() != 0 || parts.HeapBytes < total * sizeof(Sheet)) { return std::nullopt; }
  return parts;
}
}

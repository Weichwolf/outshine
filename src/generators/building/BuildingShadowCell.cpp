#include "BuildingShadowCell.h"
#include "AssetCache.h"
#include "ByteArchive.h"
#include "LevelOfDetail.h"
#include "Sha256.h"
#include "StructureArtifact.h"
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kFormat = 0x314c4c4543574453ULL;
constexpr std::string_view kKind = "building-shadow-cell";
constexpr auto kShell = static_cast<uint32_t>(LevelOfDetail::Shell);
constexpr size_t kNarrowVertices = size_t{std::numeric_limits<uint16_t>::max()} + 1u;
constexpr size_t kPositionBytes = 3u * sizeof(float);

bool WritePart(ByteWriter &out,
               std::span<const StoredVertex> vertices,
               std::span<const uint32_t> indices) {
  if (vertices.size() > std::numeric_limits<uint32_t>::max() ||
      indices.size() > std::numeric_limits<uint32_t>::max() || indices.size() % 3u != 0 ||
      !out.Number(static_cast<uint32_t>(vertices.size())) ||
      !out.Number(static_cast<uint32_t>(indices.size()))) {
    return false;
  }
  for (const auto &vertex : vertices) {
    for (const float value : vertex.pos) {
      if (!out.Number(value)) { return false; }
    }
  }
  const bool narrow = vertices.size() <= kNarrowVertices;
  for (const uint32_t index : indices) {
    if (index >= vertices.size() ||
        !(narrow ? out.Number(static_cast<uint16_t>(index)) : out.Number(index))) {
      return false;
    }
  }
  return true;
}

std::optional<std::vector<uint8_t>> Encode(const BakedTile &tile, uint32_t cell) {
  if (tile.FallbackHeights || tile.SelectedView || tile.RequestedDetail != LevelOfDetail::Shell ||
      tile.RequestedCell != cell || tile.OccupiedCells != (uint64_t{1} << (cell - 1u)) ||
      !tile.SurfaceError || tile.SurfaceFailure) {
    return std::nullopt;
  }
  ByteWriter out(kStructureArtifactBytesMost);
  if (!out.Number(kFormat) || !out.Number(tile.Digest) ||
      !out.Number(tile.SurfaceError->UpperDistanceM()) ||
      !WritePart(out, tile.Built.WallCorners, tile.Walls.DrawIndices(tile.Built.WallRun)) ||
      !WritePart(out, tile.Built.RoofCorners, tile.Roofs.DrawIndices(tile.Built.RoofRun))) {
    return std::nullopt;
  }
  return std::move(out).TakeBytes();
}

bool ReadPart(ByteReader &reader, BuildingShadowCell::Part &part) {
  uint32_t vertices = 0;
  uint32_t indices = 0;
  if (!reader.Number(vertices) || !reader.Number(indices) || indices % 3u != 0 ||
      vertices > reader.Remaining() / kPositionBytes) {
    return false;
  }
  const bool narrow = vertices <= kNarrowVertices;
  const size_t positionBytes = static_cast<size_t>(vertices) * kPositionBytes;
  const size_t indexBytes = narrow ? sizeof(uint16_t) : sizeof(uint32_t);
  if (indices > (reader.Remaining() - positionBytes) / indexBytes) { return false; }
  part.Vertices.resize(vertices, StoredVertex{});
  for (auto &vertex : part.Vertices) {
    for (float &value : vertex.pos) {
      if (!reader.Number(value) || !std::isfinite(value)) { return false; }
    }
  }
  part.Indices.resize(indices);
  for (auto &index : part.Indices) {
    uint16_t compact = 0;
    if (!(narrow ? reader.Number(compact) : reader.Number(index))) { return false; }
    if (narrow) { index = compact; }
    if (index >= vertices) { return false; }
  }
  return true;
}

std::optional<BuildingShadowCell> Decode(std::span<const uint8_t> bytes,
                                         const AssetRecord &record) {
  ByteReader in(bytes);
  uint64_t format = 0;
  BuildingShadowCell result{.Key = record.Key, .Bounds = record.Bounds, .Parts = {}};
  if (!in.Number(format) || format != kFormat || !in.Number(result.Digest) ||
      !in.Number(result.ErrorM) || !std::isfinite(result.ErrorM) || result.ErrorM < 0) {
    return std::nullopt;
  }
  for (auto &part : result.Parts) {
    if (!ReadPart(in, part)) { return std::nullopt; }
  }
  return in.Remaining() == 0 ? std::optional{std::move(result)} : std::nullopt;
}

bool KeepTransient(AssetCacheError error) {
  return error == AssetCacheError::Busy || error == AssetCacheError::CapacityExceeded;
}

std::optional<BuildingShadowCell>
Build(AssetCache &cache, AssetRecord record, const BakedTile &tile, uint32_t cell) {
  auto bytes = Encode(tile, cell);
  if (!bytes) { return std::nullopt; }
  record.ByteCount = bytes->size();
  auto result = Decode(*bytes, record);
  if (!result) { return std::nullopt; }
  auto write = cache.PreparePublication({std::move(record)}, std::move(*bytes));
  if (!write) { return KeepTransient(write.error()) ? std::move(result) : std::nullopt; }
  const auto stored = cache.Publish(*write);
  if (!stored && !KeepTransient(stored.error())) { return std::nullopt; }
  return result;
}

std::optional<BuildingShadowCell> ResolveOriginal(AssetCache &cache,
                                                  AssetRecord record,
                                                  uint32_t cell,
                                                  std::span<const std::string> keys,
                                                  BuildingShadowCell::ReadCost &cost) {
  for (const auto &key : keys) {
    const auto original = cache.Load(key, kStructureArtifactBytesMost);
    if (!original) { return std::nullopt; }
    if (!*original) { continue; }
    cost.OriginalBytes += (**original).Bytes().size();
    const auto &entry = (**original).Record();
    if (entry.Parent != record.Parent || entry.Kind != "building-static-lod" ||
        entry.Level != kShell) {
      continue;
    }
    const auto tile = DecodeStructureArtifact((**original).Bytes(), key, 1);
    if (!tile) { continue; }
    record.Bounds = entry.Bounds;
    return Build(cache, std::move(record), *tile, cell);
  }
  return std::nullopt;
}
}

std::optional<BuildingShadowCell>
BuildingShadowCell::Resolve(AssetCache &cache,
                            const std::string &parent,
                            uint32_t cell,
                            std::span<const std::string> originalKeys,
                            ReadCost &cost) {
  if (originalKeys.empty() || originalKeys.front().empty() || cell == 0 ||
      cell > kStructureCellsPerTile) {
    return std::nullopt;
  }
  const auto key = Sha256Hex("building-shadow-cell-1/" + originalKeys.front());
  const auto read = cache.Load(key, kStructureArtifactBytesMost);
  if (read && *read) {
    cost.PacketBytes += (**read).Bytes().size();
    const auto &record = (**read).Record();
    if (record.Kind == kKind && record.Parent == parent && record.Level == kShell) {
      if (auto ready = Decode((**read).Bytes(), record)) { return ready; }
    }
  }
  if (!read) { return std::nullopt; }
  return ResolveOriginal(cache,
                         {.Key = key,
                          .Kind = std::string(kKind),
                          .Bounds = {},
                          .Package = {},
                          .Level = kShell,
                          .Parent = parent},
                         cell,
                         originalKeys,
                         cost);
}
}

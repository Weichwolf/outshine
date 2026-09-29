#include "ArtifactBlocks.h"
#include "Sha256.h"
#include <algorithm>
#include <limits>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine::Data {
namespace {
constexpr std::string_view kMagic = "OSARTF01";
constexpr size_t kHashBytes = 64;
constexpr size_t kEntryBytes = kHashBytes + sizeof(uint64_t);
constexpr size_t kHeaderBytes = kMagic.size() + kHashBytes + 2 * sizeof(uint64_t);
constexpr size_t kBlocksMost =
    (kArtifactManifestBytesMost - kHeaderBytes - kHashBytes) / kEntryBytes;

bool ValidKey(std::string_view key) {
  return key.size() == kHashBytes && std::ranges::all_of(key, [](char value) {
           return (value >= '0' && value <= '9') || (value >= 'a' && value <= 'f');
         });
}

void Word(std::vector<uint8_t> &bytes, uint64_t word) {
  for (size_t i = 0; i < sizeof(word); ++i) {
    bytes.push_back(static_cast<uint8_t>(word >> (i * 8)));
  }
}

uint64_t Word(std::span<const uint8_t> &bytes) {
  uint64_t word = 0;
  for (size_t i = 0; i < sizeof(word); ++i) { word |= static_cast<uint64_t>(bytes[i]) << (i * 8); }
  bytes = bytes.subspan(sizeof(word));
  return word;
}

bool Valid(const ArtifactManifest &manifest, ArtifactLimits limits) {
  if (limits.BlockBytes == 0 || limits.EncodedBytesMost == 0 || manifest.Blocks.empty() ||
      manifest.Blocks.size() > kBlocksMost || manifest.Bytes > limits.EncodedBytesMost) {
    return false;
  }
  uint64_t total = 0;
  for (const auto &block : manifest.Blocks) {
    if (!ValidKey(block.Key) || block.Bytes == 0 || block.Bytes > limits.BlockBytes ||
        block.Bytes > limits.EncodedBytesMost - total) {
      return false;
    }
    total += block.Bytes;
  }
  return total == manifest.Bytes;
}
}

std::optional<std::vector<uint8_t>> EncodeArtifactManifest(const ArtifactManifest &manifest,
                                                           std::string_view key,
                                                           ArtifactLimits limits) {
  if (!ValidKey(key) || !Valid(manifest, limits)) { return std::nullopt; }
  std::vector<uint8_t> bytes;
  bytes.reserve(kHeaderBytes + manifest.Blocks.size() * kEntryBytes + kHashBytes);
  bytes.insert(bytes.end(), kMagic.begin(), kMagic.end());
  bytes.insert(bytes.end(), key.begin(), key.end());
  Word(bytes, manifest.Bytes);
  Word(bytes, manifest.Blocks.size());
  for (const auto &block : manifest.Blocks) {
    bytes.insert(bytes.end(), block.Key.begin(), block.Key.end());
    Word(bytes, block.Bytes);
  }
  const auto checksum = Sha256Hex(bytes.data(), bytes.size());
  bytes.insert(bytes.end(), checksum.begin(), checksum.end());
  return bytes;
}

std::optional<ArtifactManifest> DecodeArtifactManifest(std::span<const uint8_t> bytes,
                                                       std::string_view key,
                                                       ArtifactLimits limits) {
  if (!ValidKey(key) || bytes.size() < kHeaderBytes + kHashBytes ||
      bytes.size() > kArtifactManifestBytesMost ||
      !std::equal(kMagic.begin(), kMagic.end(), bytes.begin()) ||
      !std::equal(key.begin(), key.end(), bytes.begin() + kMagic.size())) {
    return std::nullopt;
  }
  const auto checksum = Sha256Hex(bytes.data(), bytes.size() - kHashBytes);
  if (!std::equal(checksum.begin(), checksum.end(), bytes.end() - kHashBytes)) {
    return std::nullopt;
  }
  bytes = bytes.subspan(kMagic.size() + kHashBytes);
  ArtifactManifest manifest;
  manifest.Bytes = Word(bytes);
  const uint64_t count = Word(bytes);
  if (count == 0 || count > kBlocksMost || bytes.size() != count * kEntryBytes + kHashBytes) {
    return std::nullopt;
  }
  manifest.Blocks.reserve(static_cast<size_t>(count));
  for (uint64_t i = 0; i < count; ++i) {
    std::string blockKey(bytes.begin(), bytes.begin() + kHashBytes);
    bytes = bytes.subspan(kHashBytes);
    const auto size = Word(bytes);
    manifest.Blocks.push_back({.Key = std::move(blockKey), .Bytes = size});
  }
  if (!Valid(manifest, limits)) { return std::nullopt; }
  return manifest;
}

ArtifactBlockWriter::ArtifactBlockWriter(ArtifactLimits limits, Publish publish)
    : Limits_(limits), Publish_(std::move(publish)) {}

bool ArtifactBlockWriter::Append(std::span<const uint8_t> bytes) {
  if (!Publish_ || bytes.empty() || bytes.size() > Limits_.BlockBytes ||
      Manifest_.Bytes > Limits_.EncodedBytesMost ||
      bytes.size() > Limits_.EncodedBytesMost - Manifest_.Bytes ||
      Manifest_.Blocks.size() >= kBlocksMost) {
    return false;
  }
  auto key = Sha256Hex(bytes.data(), bytes.size());
  if (!Publish_(key, bytes)) { return false; }
  Manifest_.Blocks.push_back({.Key = std::move(key), .Bytes = bytes.size()});
  Manifest_.Bytes += bytes.size();
  return true;
}

ArtifactBlockReader::ArtifactBlockReader(ArtifactManifest manifest, Load load)
    : Manifest_(std::move(manifest)), Load_(std::move(load)) {}

bool ArtifactBlockReader::Read(std::span<uint8_t> into) {
  while (!into.empty()) {
    if (Offset_ == Block_.size()) {
      if (!Load_ || Next_ == Manifest_.Blocks.size()) { return false; }
      const auto &entry = Manifest_.Blocks[Next_];
      if (entry.Bytes > std::numeric_limits<size_t>::max()) { return false; }
      Block_ = std::vector<uint8_t>{};
      auto block = Load_(entry.Key, static_cast<size_t>(entry.Bytes));
      if (!block || block->size() != entry.Bytes ||
          Sha256Hex(block->data(), block->size()) != entry.Key) {
        return false;
      }
      Block_ = std::move(*block);
      Offset_ = 0;
      ++Next_;
    }
    const size_t count = std::min(into.size(), Block_.size() - Offset_);
    std::copy_n(Block_.begin() + static_cast<ptrdiff_t>(Offset_), count, into.begin());
    Offset_ += count;
    into = into.subspan(count);
  }
  return true;
}
}

#include "PmTilesDirectory.h"
#include "ByteArchive.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#define ZLIB_CONST
#include <zlib.h>

namespace outshine::Generators::Terrain {
namespace {

constexpr size_t kHeaderBytes = 127;
constexpr size_t kCompressionOffset = 97;
constexpr int kGzipWindowBits = MAX_WBITS + 16;
constexpr size_t kEntriesMost = size_t{16} * 1024;
constexpr size_t kVarintBytesMost = 10;
constexpr size_t kDecodedBytesMost = (4 * kEntriesMost + 1) * kVarintBytesMost;
constexpr uint8_t kPayloadBits = 0x7f;
constexpr size_t kLastShift = 63;

bool Fits(Data::ByteRange range, uint64_t sectionBytes) {
  return range.First <= sectionBytes && range.Length <= sectionBytes - range.First;
}

std::optional<uint64_t> Varint(ByteReader &reader) {
  uint64_t value = 0;
  for (size_t shift = 0; shift <= kLastShift; shift += 7) {
    uint8_t byte = 0;
    if (!reader.Number(byte) || (shift == kLastShift && byte > 1)) { return std::nullopt; }
    value |= static_cast<uint64_t>(byte & kPayloadBits) << shift;
    if (byte <= kPayloadBits) { return value; }
  }
  return std::nullopt;
}

std::optional<std::vector<uint8_t>> Gunzip(std::span<const uint8_t> bytes) {
  std::vector<uint8_t> out(kDecodedBytesMost);
  z_stream stream{};
  if (inflateInit2(&stream, kGzipWindowBits) != Z_OK) { return std::nullopt; }
  stream.next_in = bytes.data();
  stream.avail_in = static_cast<uInt>(bytes.size());
  stream.next_out = out.data();
  stream.avail_out = static_cast<uInt>(out.size());
  const int status = inflate(&stream, Z_FINISH);
  const size_t outputSize = stream.total_out;
  const bool complete = status == Z_STREAM_END && stream.avail_in == 0;
  (void)inflateEnd(&stream);
  if (!complete) { return std::nullopt; }
  out.resize(outputSize);
  return out;
}

bool ReadIds(ByteReader &reader, std::span<PmTilesDirectory::Entry> entries) {
  uint64_t previous = 0;
  for (auto &entry : entries) {
    const auto delta = Varint(reader);
    if (!delta || *delta > std::numeric_limits<uint64_t>::max() - previous ||
        (*delta == 0 && &entry != entries.data())) {
      return false;
    }
    previous += *delta;
    entry.TileId = previous;
  }
  return true;
}

bool ReadLocations(ByteReader &reader,
                   std::span<PmTilesDirectory::Entry> entries,
                   const PmTilesDirectory::Layout &layout) {
  uint64_t nextOffset = 0;
  for (auto &entry : entries) {
    const auto offset = Varint(reader);
    if (!offset || (*offset == 0 && &entry == entries.data())) { return false; }
    entry.Bytes.First = *offset > 0 ? *offset - 1 : nextOffset;
    const auto section = entry.Run > 0 ? layout.Tiles.Length : layout.Leaves.Length;
    if (!entry.Bytes.Valid() || !Fits(entry.Bytes, section)) { return false; }
    nextOffset = entry.Bytes.First + entry.Bytes.Length;
  }
  return true;
}

bool ReadRuns(ByteReader &reader, std::span<PmTilesDirectory::Entry> entries) {
  for (size_t index = 0; index < entries.size(); ++index) {
    auto &entry = entries[index];
    const auto run = Varint(reader);
    if (!run || *run > std::numeric_limits<uint64_t>::max() - entry.TileId ||
        (index + 1 < entries.size() && *run > entries[index + 1].TileId - entry.TileId)) {
      return false;
    }
    entry.Run = *run;
  }
  return true;
}

}

std::optional<PmTilesDirectory::Layout>
PmTilesDirectory::ReadLayout(std::span<const uint8_t> prefix, uint64_t archiveBytes) {
  constexpr std::array<uint8_t, 8> magic{'P', 'M', 'T', 'i', 'l', 'e', 's', 3};
  constexpr uint8_t kNoCompression = 1;
  constexpr uint8_t kGzip = 2;
  constexpr uint8_t kWebp = 4;
  if (prefix.size() < kHeaderBytes || !std::ranges::equal(prefix.first(magic.size()), magic)) {
    return std::nullopt;
  }
  ByteReader sections(prefix.subspan(magic.size()));
  Layout layout;
  if (!sections.Number(layout.Root.First) || !sections.Number(layout.Root.Length) ||
      !sections.Take(2 * sizeof(uint64_t)) || !sections.Number(layout.Leaves.First) ||
      !sections.Number(layout.Leaves.Length) || !sections.Number(layout.Tiles.First) ||
      !sections.Number(layout.Tiles.Length) || !layout.Root.Valid() ||
      layout.Root.First < kHeaderBytes ||
      !Fits(layout.Root, std::min(prefix.size(), RootWindowBytes)) ||
      !Fits(layout.Root, archiveBytes) || !Fits(layout.Leaves, archiveBytes) ||
      !Fits(layout.Tiles, archiveBytes)) {
    return std::nullopt;
  }
  const auto compression = prefix[kCompressionOffset];
  if ((compression != kNoCompression && compression != kGzip) ||
      prefix[kCompressionOffset + 2] != kWebp) {
    return std::nullopt;
  }
  layout.Gzip = compression == kGzip;
  layout.MinimumZoom = prefix[kCompressionOffset + 3];
  layout.MaximumZoom = prefix[kCompressionOffset + 4];
  if (layout.MinimumZoom > layout.MaximumZoom || layout.MaximumZoom > Data::TileId::MaximumZoom) {
    return std::nullopt;
  }
  return layout;
}

std::optional<PmTilesDirectory> PmTilesDirectory::Read(std::span<const uint8_t> bytes,
                                                       const Layout &layout) {
  if (bytes.empty() || bytes.size() > DirectoryBytesMost) { return std::nullopt; }
  std::optional<std::vector<uint8_t>> unpacked;
  if (layout.Gzip) {
    unpacked = Gunzip(bytes);
    if (!unpacked) { return std::nullopt; }
    bytes = *unpacked;
  }
  ByteReader reader(bytes);
  const auto count = Varint(reader);
  if (!count || *count > kEntriesMost || *count > reader.Remaining() / 4) { return std::nullopt; }
  PmTilesDirectory directory;
  directory.Entries_.resize(static_cast<size_t>(*count));
  if (!ReadIds(reader, directory.Entries_) || !ReadRuns(reader, directory.Entries_)) {
    return std::nullopt;
  }
  for (auto &entry : directory.Entries_) {
    const auto length = Varint(reader);
    if (!length) { return std::nullopt; }
    entry.Bytes.Length = *length;
  }
  if (!ReadLocations(reader, directory.Entries_, layout) || reader.Remaining() != 0) {
    return std::nullopt;
  }
  return directory;
}

uint64_t PmTilesDirectory::TileId(Data::TileId tile) noexcept {
  uint32_t side = uint32_t{1} << static_cast<unsigned>(tile.Zoom);
  uint64_t id = (static_cast<uint64_t>(side) * side - 1) / 3;
  for (side >>= 1; side != 0; side >>= 1) {
    const bool xBit = (tile.X & side) != 0;
    const bool yBit = (tile.Y & side) != 0;
    id += static_cast<uint64_t>(side) * side *
          ((3u * static_cast<unsigned>(xBit)) ^ static_cast<unsigned>(yBit));
    if (!yBit) {
      if (xBit) {
        tile.X = ~tile.X;
        tile.Y = ~tile.Y;
      }
      std::swap(tile.X, tile.Y);
    }
  }
  return id;
}

const PmTilesDirectory::Entry *PmTilesDirectory::Find(uint64_t id) const noexcept {
  const auto after = std::ranges::upper_bound(Entries_, id, {}, &Entry::TileId);
  if (after == Entries_.begin()) { return nullptr; }
  const auto &candidate = *std::prev(after);
  return candidate.Run == 0 || id - candidate.TileId < candidate.Run ? &candidate : nullptr;
}

}

#include "Png.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <string_view>

#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

namespace outshine::Io {

constexpr uint32_t kByteMask = 0xffu;

constexpr unsigned kByteShift = 8u;

namespace {

constexpr std::array<uint8_t, 8> kSignature = {{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a}};
constexpr size_t kMaxSide = 16384;
constexpr size_t kMaxPaletteBytes = size_t{256} * 3;
constexpr unsigned kAncillaryBit = 0x20u;
constexpr uint32_t kMaxChunkBytes = 0x7fffffffu;

uint32_t Big(const uint8_t *at) {
  return (static_cast<uint32_t>(at[0]) << (3u * kByteShift)) |
         (static_cast<uint32_t>(at[1]) << (2u * kByteShift)) |
         (static_cast<uint32_t>(at[2]) << kByteShift) | static_cast<uint32_t>(at[3]);
}

int Paeth(int left, int above, int corner) {
  const int guess = left + above - corner;
  const int toLeft = guess > left ? guess - left : left - guess;
  const int toAbove = guess > above ? guess - above : above - guess;
  const int toCorner = guess > corner ? guess - corner : corner - guess;
  if (toLeft <= toAbove && toLeft <= toCorner) { return left; }
  return toAbove <= toCorner ? above : corner;
}

Png Refuse(std::string why) {
  Png out;
  out.Error = std::move(why);
  return out;
}

struct ImageShape {
  uint32_t Wide = 0;
  uint32_t High = 0;
  uint32_t Channels = 0;
};

struct EncodedPng {
  ImageShape Shape;
  std::vector<uint8_t> Compressed;
  bool HasPalette = false;
};

enum class ChunkPhase { Header, BeforeData, Data, AfterData };

namespace Says {
constexpr auto Chunk = "PNG chunk length, CRC or ordering is invalid";
constexpr auto Header =
    "PNG requires a bounded noninterlaced 8-bit RGB/RGBA IHDR with standard methods";
constexpr auto End = "PNG requires image data followed by a terminal empty IEND";
}

std::string ReadHeader(std::span<const uint8_t> bytes, ImageShape &shape) {
  if (bytes.size() != 13) { return Says::Header; }
  const uint32_t wide = Big(bytes.data());
  const uint32_t high = Big(bytes.data() + 4);
  if (wide == 0 || high == 0 || wide > kMaxSide || high > kMaxSide || bytes[8] != 8 ||
      (bytes[9] != 2 && bytes[9] != 6) || bytes[10] != 0 || bytes[11] != 0 || bytes[12] != 0) {
    return Says::Header;
  }
  shape = {.Wide = wide, .High = high, .Channels = bytes[9] == 2 ? 3u : 4u};
  return {};
}

std::string ReadChunk(std::string_view name,
                      std::span<const uint8_t> data,
                      EncodedPng &image,
                      ChunkPhase &phase) {
  if (phase == ChunkPhase::Header) {
    if (name != "IHDR") { return Says::Chunk; }
    auto error = ReadHeader(data, image.Shape);
    if (!error.empty()) { return error; }
    phase = ChunkPhase::BeforeData;
    return {};
  }
  if (name == "IHDR") { return Says::Chunk; }
  if (name == "IDAT") {
    if (phase == ChunkPhase::AfterData) { return Says::Chunk; }
    image.Compressed.insert(image.Compressed.end(), data.begin(), data.end());
    phase = ChunkPhase::Data;
    return {};
  }
  if (name == "PLTE") {
    if (phase != ChunkPhase::BeforeData || image.HasPalette || data.empty() ||
        data.size() > kMaxPaletteBytes || data.size() % 3 != 0) {
      return Says::Chunk;
    }
    image.HasPalette = true;
    return {};
  }
  if ((static_cast<unsigned char>(name[0]) & kAncillaryBit) == 0) { return Says::Chunk; }
  if (phase == ChunkPhase::Data) { phase = ChunkPhase::AfterData; }
  return {};
}

std::string ReadChunks(std::span<const uint8_t> bytes, EncodedPng &image) {
  ChunkPhase phase = ChunkPhase::Header;
  size_t at = kSignature.size();
  while (bytes.size() - at >= 12) {
    const uint32_t size = Big(bytes.data() + at);
    if (size > kMaxChunkBytes || size > bytes.size() - at - 12) { return Says::Chunk; }
    const size_t from = at + 8;
    const uint8_t *name = bytes.data() + at + 4;
    const auto checksum = crc32(0, name, static_cast<uInt>(size + 4));
    if (checksum != Big(bytes.data() + from + size)) { return Says::Chunk; }
    const std::string_view type(reinterpret_cast<const char *>(name), 4);
    at = from + size + 4;
    if (type == "IEND") {
      return size == 0 && !image.Compressed.empty() && at == bytes.size() ? std::string{}
                                                                          : Says::End;
    }
    auto error = ReadChunk(type, bytes.subspan(from, size), image, phase);
    if (!error.empty()) { return error; }
  }
  return Says::End;
}

Png Reconstruct(const std::vector<uint8_t> &raw, ImageShape shape) {
  const auto [wide, high, channels] = shape;
  const size_t stride = static_cast<size_t>(wide) * channels;
  Png out;
  out.Wide = wide;
  out.High = high;
  out.Channels = channels;
  out.Bytes.assign(static_cast<size_t>(high) * stride, 0);

  for (uint32_t row = 0; row < high; ++row) {
    const uint8_t filter = raw[(stride + 1) * static_cast<size_t>(row)];
    const uint8_t *in = raw.data() + (stride + 1) * static_cast<size_t>(row) + 1;
    uint8_t *outRow = out.Bytes.data() + stride * static_cast<size_t>(row);
    const uint8_t *above =
        row > 0 ? out.Bytes.data() + stride * static_cast<size_t>(row - 1) : nullptr;

    for (size_t byte = 0; byte < stride; ++byte) {
      const int left = byte >= channels ? outRow[byte - channels] : 0;
      const int up = above != nullptr ? above[byte] : 0;
      const int corner = (above != nullptr && byte >= channels) ? above[byte - channels] : 0;
      int value = in[byte];
      switch (filter) {
        case 0: break;
        case 1: value += left; break;
        case 2: value += up; break;
        case 3: value += (left + up) / 2; break;
        case 4: value += Paeth(left, up, corner); break;
        default:
          return Refuse("row " + std::to_string(row) + " declares filter " +
                        std::to_string(static_cast<int>(filter)) + ", and PNG has five");
      }
      outRow[byte] = static_cast<uint8_t>(static_cast<uint32_t>(value) & kByteMask);
    }
  }

  out.Read = true;
  return out;
}

}

Png ReadPng(const uint8_t *bytes, size_t length) {
  if (bytes == nullptr || length < sizeof(kSignature) + 12) {
    return Refuse("a PNG is at least a signature and one chunk, and this is " +
                  std::to_string(length) + " bytes");
  }
  if (std::memcmp(bytes, kSignature.data(), sizeof(kSignature)) != 0) {
    return Refuse("these bytes do not begin with the PNG signature, so they are not a PNG");
  }
  EncodedPng image;
  auto error = ReadChunks({bytes, length}, image);
  if (!error.empty()) { return Refuse(std::move(error)); }
  const size_t stride = static_cast<size_t>(image.Shape.Wide) * image.Shape.Channels;
  const size_t wanted = (stride + 1) * image.Shape.High;
  std::vector<uint8_t> raw(wanted);
  auto got = static_cast<uLongf>(wanted);
  const int how = uncompress(
      raw.data(), &got, image.Compressed.data(), static_cast<uLong>(image.Compressed.size()));
  if (how != Z_OK || static_cast<size_t>(got) != wanted) {
    return Refuse("the IDAT inflated to " + std::to_string(static_cast<size_t>(got)) +
                  " bytes where " + std::to_string(wanted) + " were needed, and zlib said " +
                  std::to_string(how));
  }
  return Reconstruct(raw, image.Shape);
}

}

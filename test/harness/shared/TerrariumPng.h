#ifndef OUTSHINE_TEST_TERRARIUMPNG_H
#define OUTSHINE_TEST_TERRARIUMPNG_H

#include <cstdint>
#include <cstring>
#include <vector>

#include <zlib.h>

namespace outshine::Test {

// a Terrarium-encoded PNG the tile layer can read: height = (r*256 + g + b/256) - 32768,
// written here so the tiles suite owns its fixtures instead of borrowing a corpus
inline std::vector<uint8_t> TerrariumPng(uint32_t width, uint32_t height,
                                         const std::vector<float> &metres) {
  const auto be32 = [](std::vector<uint8_t> &into, uint32_t v) {
    into.push_back((uint8_t)(v >> 24));
    into.push_back((uint8_t)(v >> 16));
    into.push_back((uint8_t)(v >> 8));
    into.push_back((uint8_t)v);
  };
  const auto chunk = [&](std::vector<uint8_t> &into, const char *kind,
                         const std::vector<uint8_t> &body) {
    be32(into, (uint32_t)body.size());
    std::vector<uint8_t> crcOver(kind, kind + 4);
    crcOver.insert(crcOver.end(), body.begin(), body.end());
    into.insert(into.end(), crcOver.begin(), crcOver.end());
    be32(into, (uint32_t)crc32(0, crcOver.data(), (uInt)crcOver.size()));
  };

  std::vector<uint8_t> raw;
  raw.reserve((size_t)height * (1u + (size_t)width * 3u));
  for (uint32_t r = 0; r < height; ++r) {
    raw.push_back(0); // filter: none
    for (uint32_t c = 0; c < width; ++c) {
      const double m = (double)metres[(size_t)r * width + c] + 32768.0;
      const double clamped = m < 0.0 ? 0.0 : (m > 65535.0 ? 65535.0 : m);
      const uint32_t whole = (uint32_t)clamped;
      raw.push_back((uint8_t)(whole >> 8));
      raw.push_back((uint8_t)(whole & 0xff));
      raw.push_back((uint8_t)((clamped - (double)whole) * 256.0));
    }
  }
  uLongf squeezedBytes = compressBound((uLong)raw.size());
  std::vector<uint8_t> squeezed(squeezedBytes);
  compress2(squeezed.data(), &squeezedBytes, raw.data(), (uLong)raw.size(), 6);
  squeezed.resize(squeezedBytes);

  std::vector<uint8_t> png = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
  std::vector<uint8_t> ihdr;
  be32(ihdr, width);
  be32(ihdr, height);
  ihdr.push_back(8); // bit depth
  ihdr.push_back(2); // colour type: truecolour
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(0);
  chunk(png, "IHDR", ihdr);
  chunk(png, "IDAT", squeezed);
  chunk(png, "IEND", {});
  return png;
}

}
#endif

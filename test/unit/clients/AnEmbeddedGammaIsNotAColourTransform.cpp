#include <cstdint>
#include <string>
#include <vector>

#include "Check.h"

#include "Image.h"

using outshine::Clients::DecodeImage;
using outshine::Clients::EncodePng;
using outshine::Clients::Raster;

namespace {

void Big32(std::vector<uint8_t> &out, uint32_t value) {
  out.push_back(static_cast<uint8_t>(value >> 24));
  out.push_back(static_cast<uint8_t>(value >> 16));
  out.push_back(static_cast<uint8_t>(value >> 8));
  out.push_back(static_cast<uint8_t>(value));
}

uint32_t Crc32(const uint8_t *bytes, size_t count) {
  uint32_t crc = 0xFFFFFFFFu;
  for (size_t i = 0; i < count; ++i) {
    crc ^= bytes[i];
    for (int bit = 0; bit < 8; ++bit) {
      crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
  }
  return crc ^ 0xFFFFFFFFu;
}

void Chunk(std::vector<uint8_t> &out, const char *type, const std::vector<uint8_t> &payload) {
  Big32(out, static_cast<uint32_t>(payload.size()));
  std::vector<uint8_t> typed(type, type + 4);
  typed.insert(typed.end(), payload.begin(), payload.end());
  out.insert(out.end(), typed.begin(), typed.end());
  Big32(out, Crc32(typed.data(), typed.size()));
}

std::vector<uint8_t> ZlibStored(const std::vector<uint8_t> &raw) {
  std::vector<uint8_t> out{0x78, 0x01};
  size_t at = 0;
  do {
    const size_t take = (raw.size() - at < 65535u) ? raw.size() - at : 65535u;
    const bool last = (at + take == raw.size());
    out.push_back(last ? 1 : 0);
    out.push_back(static_cast<uint8_t>(take));
    out.push_back(static_cast<uint8_t>(take >> 8));
    out.push_back(static_cast<uint8_t>(~take));
    out.push_back(static_cast<uint8_t>(~take >> 8));
    out.insert(out.end(), raw.begin() + static_cast<long>(at), raw.begin() + static_cast<long>(at + take));
    at += take;
  } while (at < raw.size());
  uint32_t a = 1, b = 0;
  for (const uint8_t byte : raw) {
    a = (a + byte) % 65521u;
    b = (b + a) % 65521u;
  }
  Big32(out, (b << 16) | a);
  return out;
}

std::vector<uint8_t> Png(const std::vector<uint8_t> &colourChunk) {
  constexpr int kSide = 2;
  const uint8_t pixel[3] = {128, 64, 192};
  std::vector<uint8_t> scanlines;
  for (int row = 0; row < kSide; ++row) {
    scanlines.push_back(0);
    for (int column = 0; column < kSide; ++column) {
      scanlines.insert(scanlines.end(), pixel, pixel + 3);
    }
  }
  std::vector<uint8_t> header;
  Big32(header, kSide);
  Big32(header, kSide);
  header.push_back(8);
  header.push_back(2);
  header.push_back(0);
  header.push_back(0);
  header.push_back(0);

  std::vector<uint8_t> out{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  Chunk(out, "IHDR", header);
  out.insert(out.end(), colourChunk.begin(), colourChunk.end());
  Chunk(out, "IDAT", ZlibStored(scanlines));
  Chunk(out, "IEND", {});
  return out;
}

std::vector<uint8_t> GammaChunk(uint32_t hundredThousandths) {
  std::vector<uint8_t> payload;
  Big32(payload, hundredThousandths);
  std::vector<uint8_t> chunk;
  Chunk(chunk, "gAMA", payload);
  return chunk;
}

std::vector<uint8_t> SrgbChunk(uint8_t intent) {
  std::vector<uint8_t> chunk;
  Chunk(chunk, "sRGB", {intent});
  return chunk;
}

bool Decoded(const std::vector<uint8_t> &png, Raster &out) {
  return DecodeImage(png.data(), png.size(), out) && out.Holds();
}

}

int main() {
  using namespace outshine::Test;

  Raster plain, linearGamma, displayGamma, srgbIntent;
  const bool allDecoded = Decoded(Png({}), plain) &&
                          Decoded(Png(GammaChunk(100000)), linearGamma) &&
                          Decoded(Png(GammaChunk(45455)), displayGamma) &&
                          Decoded(Png(SrgbChunk(0)), srgbIntent);
  CHECK(allDecoded, "four PNGs differing only in their colorimetric chunk all decode");
  if (!allDecoded) { return Report(); }

  CHECK(plain.Width == 2 && plain.Height == 2, "the decoder reports the image's own size");
  CHECK(plain.Rgba[0] == 128 && plain.Rgba[1] == 64 && plain.Rgba[2] == 192 && plain.Rgba[3] == 255,
        "an image with no alpha channel decodes to RGBA8 with alpha at full");
  Note("decoded first texel R", (double)plain.Rgba[0], "code");
  Note("decoded first texel G", (double)plain.Rgba[1], "code");
  Note("decoded first texel B", (double)plain.Rgba[2], "code");

  CHECK(linearGamma.Rgba == plain.Rgba, "a gAMA chunk of 1.0 changes no texel");
  CHECK(displayGamma.Rgba == plain.Rgba, "a gAMA chunk of 1/2.2 changes no texel either");
  CHECK(srgbIntent.Rgba == plain.Rgba, "an sRGB rendering-intent chunk changes no texel");

  {
    std::vector<uint8_t> texels;
    for (int i = 0; i < 4 * 3; ++i) { texels.push_back(static_cast<uint8_t>(i * 17 + 3)); }
    std::vector<uint8_t> encoded;
    Raster back;
    const bool roundTripped =
        EncodePng(texels.data(), 3, 1, encoded) && Decoded(encoded, back);
    CHECK(roundTripped, "RGBA8 encodes to a PNG and decodes back");
    if (roundTripped) {
      CHECK(back.Width == 3 && back.Height == 1, "the round trip keeps the raster's shape");
      CHECK(back.Rgba == texels, "and every one of its bytes, alpha included and never premultiplied");
    }
  }

  {
    Raster nothing;
    const std::vector<uint8_t> notAnImage{'n', 'o', 't', ' ', 'a', 'n', ' ', 'i', 'm', 'a', 'g', 'e'};
    CHECK(!DecodeImage(notAnImage.data(), notAnImage.size(), nothing),
          "bytes that are not an image are refused");
    CHECK(!DecodeImage(nullptr, 0, nothing), "and so is nothing at all");
    CHECK(nothing.Width == 0 && nothing.Rgba.empty(), "a refused decode leaves no raster behind");
  }

  Covers("I.26.12 khronos:TextureEncodingTest -- embedded gamma values and ICC profiles are ignored "
         "as the specification requires, so the colour space of an image is decided by the material "
         "slot that samples it and never by the file");
  return Report();
}

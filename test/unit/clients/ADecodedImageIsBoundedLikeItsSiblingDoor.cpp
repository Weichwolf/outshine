#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

#include "Check.h"

#include "Image.h"

namespace {

// a hand-written 24-bit BMP: valid enough for SDL_image, wide enough to cross the bound
[[nodiscard]] std::vector<uint8_t> Bmp(int width, int height) {
  const uint32_t rowBytes = ((uint32_t)width * 3u + 3u) & ~3u;
  const uint32_t dataBytes = rowBytes * (uint32_t)height;
  const uint32_t fileBytes = 54u + dataBytes;
  std::vector<uint8_t> out(fileBytes, 0x7f);
  const auto put32 = [&](size_t at, uint32_t v) {
    out[at] = (uint8_t)v;
    out[at + 1] = (uint8_t)(v >> 8);
    out[at + 2] = (uint8_t)(v >> 16);
    out[at + 3] = (uint8_t)(v >> 24);
  };
  out[0] = 'B';
  out[1] = 'M';
  put32(2, fileBytes);
  put32(6, 0);
  put32(10, 54);
  put32(14, 40);
  put32(18, (uint32_t)width);
  put32(22, (uint32_t)height);
  out[26] = 1;
  out[27] = 0;
  out[28] = 24;
  out[29] = 0;
  put32(30, 0);
  put32(34, dataBytes);
  put32(38, 2835);
  put32(42, 2835);
  put32(46, 0);
  put32(50, 0);
  return out;
}

} // namespace

int main(void) {
  using namespace outshine::Test;
  std::setvbuf(stdout, nullptr, _IONBF, 0);

  outshine::Clients::Raster raster;
  {
    const std::vector<uint8_t> small = Bmp(8, 8);
    CHECK(outshine::Clients::DecodeImage(small.data(), small.size(), raster) &&
              raster.Width == 8 && raster.Height == 8,
          "an eight-by-eight image decodes as itself");
  }
  {
    const std::vector<uint8_t> wide = Bmp(17000, 1);
    CHECK(!outshine::Clients::DecodeImage(wide.data(), wide.size(), raster),
          "**A SIDE PAST THE DEVICE'S OWN MAXIMUM REFUSES**: 17000 px crosses the same "
          "16384 the PNG door names, and the content boundary answers no instead of "
          "buying the allocation (board:1742)");
  }
  {
    const std::vector<uint8_t> tall = Bmp(1, 17000);
    CHECK(!outshine::Clients::DecodeImage(tall.data(), tall.size(), raster),
          "and the tall side by the same rule");
  }

  Covers("II.13 the SDL_image door bounds its output the way the PNG door does: one "
         "device-derived kMaxSide, refused on either axis before the engine's own "
         "allocations follow the decode (board:1742)");
  return Report();
}

#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>
#include <vector>

namespace outshine::Clients {

struct Raster {
  int Width = 0;
  int Height = 0;
  std::vector<uint8_t> Rgba;

  [[nodiscard]] bool Holds() const {
    return Width > 0 && Height > 0 &&
           Rgba.size() == static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u;
  }
};

[[nodiscard]] bool DecodeImage(const uint8_t *bytes, size_t count, Raster &out);

[[nodiscard]] bool EncodePng(const uint8_t *rgba, int width, int height, std::vector<uint8_t> &out);

}
#endif

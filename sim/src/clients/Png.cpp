#include "Png.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace outshine::Clients {

bool EncodePng(const uint8_t *rgba, int width, int height, std::vector<uint8_t> &out) {
  out.clear();
  const auto sink = [](void *ctx, void *data, int n) {
    std::vector<uint8_t> *v = (std::vector<uint8_t> *)ctx;
    const uint8_t *p = (const uint8_t *)data;
    v->insert(v->end(), p, p + n);
  };
  return stbi_write_png_to_func(sink, &out, width, height, 4, rgba, width * 4) != 0;
}

}  // namespace outshine::Clients

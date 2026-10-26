#ifndef OUTSHINE_APP_CAPTURE_PNGENCODING_H
#define OUTSHINE_APP_CAPTURE_PNGENCODING_H

#include <cstdint>
#include <vector>

namespace outshine::App {
[[nodiscard]] bool EncodePng(const uint8_t *rgba, int width, int height, std::vector<uint8_t> &out);
}
#endif

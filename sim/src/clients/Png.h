#ifndef PNG_H
#define PNG_H

#include <cstdint>
#include <vector>

namespace outshine::Clients {

/* RGBA8 to a PNG byte stream. Encoding is shared because the destination is not: a file natively, a
 * POST body in the browser, and neither one may re-implement the encoder. */
bool EncodePng(const uint8_t *rgba, int width, int height, std::vector<uint8_t> &out);

} // namespace outshine::Clients
#endif

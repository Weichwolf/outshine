/* THE IMAGE BOUNDARY: encoded bytes in, RGBA8 out, and RGBA8 in, PNG bytes out. Both directions are
 * here because they are one dependency and one convention, and a decoder in a second file would be a
 * second place for the alpha rule to be stated.
 *
 * ONE CONVENTION: RGBA8, STRAIGHT ALPHA, TOP ROW FIRST. Straight because premultiplied alpha is a
 * different number in the same eight bits and `AlphaBlendModeTest` exists to catch an engine that
 * confuses them; top row first because that is what a raster readback on this side produces and one
 * of the two orders had to be named.
 *
 * WHAT THE DECODER MUST NOT DO, and it is a rule the format tempts you to break: an embedded gamma
 * chunk (`gAMA`), an sRGB rendering-intent chunk (`sRGB`) and an ICC profile (`iCCP`) are IGNORED.
 * glTF says the colour space of an image is decided by the SLOT it is used in -- base colour and
 * emissive are sRGB, metallic-roughness and normal are linear -- so a decoder that helpfully applied
 * a file's own gamma would be applying a transfer twice on one slot and once on another, which is
 * exactly the failure `TextureEncodingTest` renders four columns to expose. */
#ifndef IMAGE_H
#define IMAGE_H

#include <cstdint>
#include <vector>

namespace outshine::Clients {

/* A decoded raster and its own dimensions, so a caller cannot pair one with the other's size. */
struct Raster {
  int Width = 0;
  int Height = 0;
  std::vector<uint8_t> Rgba;   /* Width * Height * 4, row-major, top row first */

  [[nodiscard]] bool Holds() const {
    return Width > 0 && Height > 0 &&
           Rgba.size() == static_cast<size_t>(Width) * static_cast<size_t>(Height) * 4u;
  }
};

/* PNG, JPEG or anything else the platform image library reads, decided by the bytes and never by a
 * file name. `false` leaves `out` empty and the reason in the platform's own error channel. */
[[nodiscard]] bool DecodeImage(const uint8_t *bytes, size_t count, Raster &out);

/* RGBA8 to a PNG byte stream. Encoding is shared because the destination is not: a file, a buffer a
 * test scores, and neither one may re-implement the encoder. */
[[nodiscard]] bool EncodePng(const uint8_t *rgba, int width, int height, std::vector<uint8_t> &out);

} // namespace outshine::Clients
#endif

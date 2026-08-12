#ifndef VERSATILESVECTOR_H
#define VERSATILESVECTOR_H

#include "WebTileSource.h"

namespace outshine::Data {

/* VERSATILES' OSM VECTOR PYRAMID, the built world and the land cover. Mapbox Vector Tiles, whose
 * layers this engine reads through world/OsmVector.
 *
 * MEASURED 2026-08-12: z14 is its last zoom and z15 answers 404, and a mid-Pacific z14 tile answers
 * 200 with a 38-byte tile rather than an absence. A vector tile CANNOT be cropped from its parent
 * the way a raster can — the geometry would be clipped, not resampled — so `AncestorFill` is false
 * here and a z15 vector request is Outside this source's domain. That is the property the selector
 * needs to hand it to another one instead of guessing. */
class VersatilesVector : public WebTileSource {
public:
  VersatilesVector();

protected:
  [[nodiscard]] std::string Url(const Address &at) const override;
  [[nodiscard]] Meaning Classify(int status, size_t bytes) const noexcept override;
};

} // namespace outshine::Data
#endif

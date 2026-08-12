#ifndef TERRARIUMDEM_H
#define TERRARIUMDEM_H

#include "WebTileSource.h"

namespace outshine::Data {

/* MAPZEN TERRARIUM ON S3, the global elevation pyramid this engine's ground is built from. RGB-coded
 * metres in a PNG; height = R*256 + G + B/256 - 32768, which is the decoder's business and not this
 * file's.
 *
 * MEASURED 2026-08-12, and it is what the status table below is written from: a mid-Pacific z15 tile
 * answers 200 with a 757-byte PNG rather than an absence, so this source is never vacant over water
 * — the dataset carries bathymetry. The only 404 it produces is an address that is not in the
 * pyramid at all, which the coverage predicate already refuses, and an address above its last
 * native zoom, which `AncestorFill` answers from the parent instead. */
class TerrariumDem : public WebTileSource {
public:
  TerrariumDem();

protected:
  [[nodiscard]] std::string Url(const Address &at) const override;
  [[nodiscard]] Meaning Classify(int status, size_t bytes) const noexcept override;
};

} // namespace outshine::Data
#endif

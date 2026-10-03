#include "TerrariumSource.h"
#include "TerrariumRaster.h"
#include <cstdint>
#include <expected>
#include <span>
#include <utility>

namespace outshine::Generators::Terrain {

TerrariumSource::TerrariumSource(const Data::SourceDecl &declaration)
    : WebTileSource(declaration, declaration.Endpoint) {}

std::expected<HeightRaster, Data::DecodeFailure>
TerrariumSource::DecodeElevation(std::span<const uint8_t> bytes) const {
  auto decoded = DecodeTerrariumWebp(bytes);
  if (!decoded) { return std::unexpected(Data::DecodeFailure::CorruptPayload); }
  return std::move(*decoded);
}

}

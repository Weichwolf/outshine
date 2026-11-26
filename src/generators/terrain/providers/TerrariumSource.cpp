#include "TerrariumSource.h"
#include "MapterhornAvailability.h"
#include "TerrariumRaster.h"
#include <cstdint>
#include <expected>
#include <memory>
#include <span>
#include <utility>

namespace outshine::Generators::Terrain {

TerrariumSource::TerrariumSource(const Data::SourceDecl &declaration)
    : WebTileSource(declaration, declaration.Endpoint),
      Availability_(declaration.Endpoint == MapterhornEndpoint
                        ? std::make_unique<MapterhornAvailability>()
                        : nullptr) {}

TerrariumSource::~TerrariumSource() = default;

TerrariumSource::AvailabilityCheck TerrariumSource::CheckAvailability(
    const Data::Address &at, Data::RawSourceCache &store, Data::SourceTransport &transport) const {
  const auto tile = at.Tile();
  return tile && Availability_ ? Availability_->Check(*tile, store, transport)
                               : AvailabilityCheck{};
}

std::expected<HeightRaster, Data::DecodeFailure>
TerrariumSource::DecodeElevation(std::span<const uint8_t> bytes) const {
  auto decoded = DecodeTerrariumWebp(bytes);
  if (!decoded) { return std::unexpected(Data::DecodeFailure::CorruptPayload); }
  return std::move(*decoded);
}

}

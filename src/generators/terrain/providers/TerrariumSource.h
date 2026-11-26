#ifndef OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_TERRARIUMSOURCE_H
#define OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_TERRARIUMSOURCE_H

#include "WebTileSource.h"
#include <SourceDecl.h>
#include <memory>
#include <string_view>

namespace outshine::Generators::Terrain {

class MapterhornAvailability;

class TerrariumSource final : public Data::WebTileSource {
public:
  static constexpr std::string_view MapterhornEndpoint =
      "https://tiles.mapterhorn.com/{z}/{x}/{y}.webp";

  explicit TerrariumSource(const Data::SourceDecl &declaration);
  ~TerrariumSource() override;

  [[nodiscard]] AvailabilityCheck
  CheckAvailability(const Data::Address &at,
                    Data::RawSourceCache &store,
                    Data::SourceTransport &transport) const override;

  [[nodiscard]] std::expected<HeightRaster, Data::DecodeFailure>
  DecodeElevation(std::span<const uint8_t> bytes) const override;

private:
  std::unique_ptr<MapterhornAvailability> Availability_;
};

}
#endif

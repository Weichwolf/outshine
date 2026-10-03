#ifndef OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_TERRARIUMSOURCE_H
#define OUTSHINE_GENERATORS_TERRAIN_PROVIDERS_TERRARIUMSOURCE_H

#include "WebTileSource.h"
#include <world/data/SourceDecl.h>

namespace outshine::Generators::Terrain {

class TerrariumSource final : public Data::WebTileSource {
public:
  explicit TerrariumSource(const Data::SourceDecl &declaration);

  [[nodiscard]] std::expected<HeightRaster, Data::DecodeFailure>
  DecodeElevation(std::span<const uint8_t> bytes) const override;
};

}
#endif

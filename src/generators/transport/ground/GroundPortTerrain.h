#ifndef OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTTERRAIN_H
#define OUTSHINE_GENERATORS_TRANSPORT_GROUND_GROUNDPORTTERRAIN_H

#include "Earth.h"
#include "TransportPlan.h"
#include <cstddef>
#include <expected>
#include <memory>
#include <optional>
#include <stop_token>
#include <string_view>

namespace outshine::Ground {
class HeightField;
}

namespace outshine::Generators {
struct GroundPortTerrainLimits {
  size_t Tiles = 64;
  size_t RasterBytes = 67108864;
  size_t MetadataBytes = 1048576;
};

struct GroundPortTerrainWork {
  size_t Fields = 0;
  size_t RasterBytes = 0;
  size_t MetadataBytes = 0;
  size_t CopiedRasterBytes = 0;
};

enum class GroundPortTerrainError { InvalidInput, MissingTerrain, CapacityExceeded, Canceled };

class GroundPortTerrain {
public:
  static constexpr std::string_view ModelHeightMapping =
      "source signed metres as model geodetic heights/1";
  [[nodiscard]] static std::expected<GroundPortTerrain, GroundPortTerrainError>
  Bind(std::shared_ptr<const Ground::HeightField> field,
       std::string_view sourceDatum,
       std::string_view heightMapping,
       GroundPortTerrainLimits limits = {},
       const std::stop_token &stop = {});

  [[nodiscard]] std::optional<double> At(LongitudeLatitude at) const noexcept;

  [[nodiscard]] const TransportInputReference &Input() const noexcept { return Input_; }

  [[nodiscard]] GroundPortTerrainWork Work() const noexcept { return Work_; }

private:
  GroundPortTerrain(std::shared_ptr<const Ground::HeightField> field,
                    TransportInputReference input,
                    GroundPortTerrainWork work);
  std::shared_ptr<const Ground::HeightField> Field_;
  TransportInputReference Input_;
  GroundPortTerrainWork Work_;
};
}
#endif

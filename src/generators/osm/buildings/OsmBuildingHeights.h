#ifndef OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMBUILDINGHEIGHTS_H
#define OUTSHINE_GENERATORS_OSM_BUILDINGS_OSMBUILDINGHEIGHTS_H

#include <expected>
#include <optional>
#include <span>
#include <cstdint>

#include "OsmElements.h"
#include "BuildingHeightInterval.h"

namespace outshine::Generators::Osm {

enum class HeightError : uint8_t {
  InvalidNumber,
  InvalidUnit,
  DuplicateTag,
  InvalidPolicy,
  InvalidInterval
};
using HeightValue = std::expected<std::optional<double>, HeightError>;

struct HeightPolicy {
  double StoreyHeightM;
  double BodyHeightM;
};

struct BuildingHeights {
  HeightValue TopM;
  HeightValue MinimumM;
  HeightValue Levels;
  HeightValue MinimumLevel;

  [[nodiscard]] static BuildingHeights Read(std::span<const Data::OsmTag> tags) noexcept;
  [[nodiscard]] std::expected<outshine::Ground::BuildingHeightInterval, HeightError>
  Resolve(HeightPolicy policy) const noexcept;
};

}

#endif

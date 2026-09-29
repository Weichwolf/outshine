#ifndef OUTSHINE_WORLD_GROUND_OSMBUILDINGHEIGHTS_H
#define OUTSHINE_WORLD_GROUND_OSMBUILDINGHEIGHTS_H

#include <expected>
#include <optional>
#include <span>
#include <cstdint>

#include "OsmElements.h"

namespace outshine::Ground {

enum class OsmHeightError : uint8_t { InvalidNumber, InvalidUnit, DuplicateTag };
using OsmHeightValue = std::expected<std::optional<double>, OsmHeightError>;

struct OsmBuildingHeights {
  OsmHeightValue TopM;
  OsmHeightValue MinimumM;
  OsmHeightValue Levels;
  OsmHeightValue MinimumLevel;

  [[nodiscard]] static OsmBuildingHeights Read(std::span<const Data::OsmTag> tags) noexcept;
};

}

#endif

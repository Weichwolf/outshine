#ifndef OUTSHINE_WORLD_GROUND_OSMBUILDINGHEIGHTS_H
#define OUTSHINE_WORLD_GROUND_OSMBUILDINGHEIGHTS_H

#include <expected>
#include <optional>
#include <span>
#include <cstdint>

#include "OsmElements.h"

namespace outshine::Ground {

enum class OsmHeightError : uint8_t {
  InvalidNumber,
  InvalidUnit,
  DuplicateTag,
  InvalidPolicy,
  InvalidInterval
};
using OsmHeightValue = std::expected<std::optional<double>, OsmHeightError>;

enum class OsmHeightOrigin : uint8_t { MetricTag, Levels, Policy };

struct OsmHeightPolicy {
  double StoreyHeightM;
  double BodyHeightM;
};

struct OsmHeightInterval {
  double TopM;
  double MinimumM;
  OsmHeightOrigin TopOrigin;
  OsmHeightOrigin MinimumOrigin;
  bool ConflictingLevels;
};

struct OsmBuildingHeights {
  OsmHeightValue TopM;
  OsmHeightValue MinimumM;
  OsmHeightValue Levels;
  OsmHeightValue MinimumLevel;

  [[nodiscard]] static OsmBuildingHeights Read(std::span<const Data::OsmTag> tags) noexcept;
  [[nodiscard]] std::expected<OsmHeightInterval, OsmHeightError>
  Resolve(OsmHeightPolicy policy) const noexcept;
};

}

#endif

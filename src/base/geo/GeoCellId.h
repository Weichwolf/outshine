#ifndef OUTSHINE_WORLD_DATA_GEOCELLID_H
#define OUTSHINE_WORLD_DATA_GEOCELLID_H

#include "GeographicBounds.h"
#include <cstdint>
#include <optional>
#include <cstddef>
#include <expected>
#include <string>
#include <vector>

namespace outshine {

struct GeoCellId {
  static constexpr int MaximumLevel = 24;
  int Level = 0;
  uint32_t X = 0;
  uint32_t Y = 0;

  [[nodiscard]] bool Valid() const noexcept {
    return Level >= 0 && Level <= MaximumLevel &&
           X < (uint32_t{1} << static_cast<unsigned>(Level)) &&
           Y < (uint32_t{1} << static_cast<unsigned>(Level));
  }

  [[nodiscard]] std::optional<GeographicBounds> Bounds() const noexcept;

  [[nodiscard]] bool operator==(const GeoCellId &) const noexcept = default;
};

[[nodiscard]] std::expected<std::vector<GeoCellId>, std::string>
CellsAround(double latitudeDeg, double longitudeDeg, double radiusM, int level, size_t cellsMost);

}

#endif

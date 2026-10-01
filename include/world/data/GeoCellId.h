#ifndef OUTSHINE_WORLD_DATA_GEOCELLID_H
#define OUTSHINE_WORLD_DATA_GEOCELLID_H

#include <world/SourceProvider.h>
#include <cstdint>
#include <optional>

namespace outshine::Data {

/// WGS84 geographic quadtree cell, independent of Mercator tiles and one-degree DEM cells.
/// Level L partitions longitude [-180,180] and latitude [-90,90] into 2^L cells per axis.
/// Adjacent cells share bounds; consumers deduplicate original objects by typed identity.
struct GeoCellId {
  static constexpr int MaximumLevel = 24; ///< Finest admitted geographic partition.
  int Level = 0;                          ///< Partition level in [0, MaximumLevel].
  uint32_t X = 0; ///< Longitude column in [0, 2^Level), increasing eastward.
  uint32_t Y = 0; ///< Latitude row in [0, 2^Level), increasing northward.

  /// Check coordinates without allocation or normalization.
  /// @return True when level and both axes belong to the declared partition.
  [[nodiscard]] bool Valid() const noexcept {
    return Level >= 0 && Level <= MaximumLevel &&
           X < (uint32_t{1} << static_cast<unsigned>(Level)) &&
           Y < (uint32_t{1} << static_cast<unsigned>(Level));
  }

  /// Derive source bounds; invalid coordinates do not produce a wrapped or clipped cell.
  /// @return Closed WGS84 bounds in degrees, or nothing; no allocation or IO.
  [[nodiscard]] std::optional<SourceCoverage> Bounds() const noexcept;

  /// Compare level and both geographic axes exactly.
  /// @return True for the same source cell; no allocation.
  [[nodiscard]] bool operator==(const GeoCellId &) const noexcept = default;
};

}

#endif

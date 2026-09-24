#ifndef OUTSHINE_WORLD_SOURCEPROVIDER_H
#define OUTSHINE_WORLD_SOURCEPROVIDER_H

#include <cstdint>
#include <optional>
#include <string>

namespace outshine::Data {

/// Action taken when a world-data source has no value for a requested address.
enum class MissingDataPolicy : uint8_t {
  Continue, ///< Try the next compatible source by increasing priority.
  Fail      ///< Stop the request and report the missing required value.
};

/// Closed geodetic coverage of one bounded source chunk, in degrees.
/// Antimeridian-crossing coverage uses two chunks. Coordinates must be finite.
struct SourceCoverage {
  double WestDeg = 0.0;  ///< Western longitude in [-180, 180].
  double SouthDeg = 0.0; ///< Southern latitude in [-90, 90].
  double EastDeg = 0.0;  ///< Eastern longitude in [-180, 180], greater than WestDeg.
  double NorthDeg = 0.0; ///< Northern latitude in [-90, 90], greater than SouthDeg.

  /// Compare the declared coverage coordinates exactly.
  [[nodiscard]] bool operator==(const SourceCoverage &) const = default;
};

/// Owned configuration for one world-data source selected by the runtime.
/// Strings are copied by value. Lower Priority values are queried first. Validation and
/// construction may allocate and perform IO; the value itself has no thread affinity.
struct SourceProvider {
  std::string Kind;     ///< Registered source category such as terrain, vector or stars.
  std::string Revision; ///< Opaque data revision included in cache identity.
  int Priority = 0;     ///< Ordering among sources of the same category; lower is earlier.
  MissingDataPolicy Missing = MissingDataPolicy::Continue; ///< Missing-value behavior.
  std::string Dataset;                    ///< Stable dataset ID; required for semantic OSM chunks.
  std::string Location;                   ///< OSM file path, absolute or relative to Roots.Shipped.
  std::optional<SourceCoverage> Coverage; ///< Required finite OSM chunk bounds.

  /// Compare the complete owned source declaration.
  [[nodiscard]] bool operator==(const SourceProvider &) const = default;
};

}

#endif

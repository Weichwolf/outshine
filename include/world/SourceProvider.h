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
  /// @return True when all four bounds are equal.
  [[nodiscard]] bool operator==(const SourceCoverage &) const = default;
};

/// Owned configuration for one world-data source selected by the runtime.
/// Strings are copied by value. Lower Priority values are queried first. Validation and
/// construction may allocate and perform IO; the value itself has no thread affinity.
struct SourceProvider {
  std::string Kind;     ///< Registered source category such as terrain, vector or stars.
  std::string Revision; ///< Shared dataset revision; legacy OSM sha256: pins verify file bytes.
  int Priority = 0;     ///< Ordering among sources of the same category; lower is earlier.
  MissingDataPolicy Missing = MissingDataPolicy::Continue; ///< Missing-value behavior.
  std::string Dataset;  ///< Stable dataset ID; required for semantic OSM chunks.
  std::string Location; ///< OSM file path, absolute or relative to Roots.Shipped.
  std::string Endpoint; ///< Source-specific HTTPS endpoint; built-in terrain uses Terrarium WebP.
  std::optional<SourceCoverage>
      Coverage; ///< OSM chunk bounds; absent for an official API catalogue.
  std::string
      PayloadSha256; ///< Optional 64-digit lowercase OSM response digest, separate from revision.

  std::string Schema =
      {}; ///< Provider-owned semantic schema; empty selects its documented default.

  /// Compare the complete owned source declaration.
  /// @return True when all source fields are equal.
  [[nodiscard]] bool operator==(const SourceProvider &) const = default;
};

}

#endif

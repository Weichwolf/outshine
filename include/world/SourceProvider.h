#ifndef OUTSHINE_WORLD_SOURCEPROVIDER_H
#define OUTSHINE_WORLD_SOURCEPROVIDER_H

#include <cstdint>
#include <string>

namespace outshine::Data {

/// Action taken when a world-data source has no value for a requested address.
enum class MissingDataPolicy : uint8_t {
  Continue, ///< Try the next compatible source by increasing priority.
  Fail      ///< Stop the request and report the missing required value.
};

/// Owned configuration for one world-data source selected by the runtime.
/// Strings are copied by value. Lower Priority values are queried first. Validation and
/// construction may allocate and perform IO; the value itself has no thread affinity.
struct SourceProvider {
  std::string Kind;     ///< Registered source category such as terrain, vector or stars.
  std::string Revision; ///< Opaque data revision included in cache identity.
  int Priority = 0;     ///< Ordering among sources of the same category; lower is earlier.
  MissingDataPolicy Missing = MissingDataPolicy::Continue; ///< Missing-value behavior.
};

}

#endif

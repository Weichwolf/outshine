#ifndef OUTSHINE_WORLD_DATA_DATAKIND_H
#define OUTSHINE_WORLD_DATA_DATAKIND_H

#include <cstdint>

namespace outshine::Data {

/// Native consumer category, independent of the provider registration name.
enum class DataKind : uint8_t {
  Elevation,     ///< Meter-valued terrain source.
  VectorMap,     ///< Explicit legacy vector-tile fixture; not original OSM.
  StarCatalogue, ///< Star-band catalogue product.
  OriginalOsm    ///< Original OSM nodes, ways, relations and tags.
};

/// Describe a data category without allocation.
/// @param kind Category value.
/// @return Process-lifetime name or an empty string for an unsupported value.
[[nodiscard]] const char *Name(DataKind kind) noexcept;

}
#endif

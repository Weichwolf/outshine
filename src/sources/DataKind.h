#ifndef OUTSHINE_SOURCES_DATAKIND_H
#define OUTSHINE_SOURCES_DATAKIND_H

#include <cstdint>

namespace outshine::Data {

enum class DataKind : uint8_t { Elevation, VectorMap, StarCatalogue, OriginalOsm, Weather };

[[nodiscard]] const char *Name(DataKind kind) noexcept;

}
#endif

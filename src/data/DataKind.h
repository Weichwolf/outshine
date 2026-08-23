#ifndef OUTSHINE_DATA_DATAKIND_H
#define OUTSHINE_DATA_DATAKIND_H

#include <cstdint>

namespace outshine::Data {

enum class DataKind : uint8_t { Elevation, VectorMap, StarCatalogue };

[[nodiscard]] const char *Name(DataKind kind) noexcept;

}
#endif

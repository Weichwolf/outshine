#ifndef DATAKIND_H
#define DATAKIND_H

#include <cstdint>

namespace outshine::Data {

/* WHAT A REQUEST IS ABOUT, and the first filter the selector applies. Not a content taxonomy: an
 * elevation upstream and a vector upstream answer different questions about the same place, and no
 * source can stand in for the other, so the kind is what makes two sources comparable at all. */
enum class DataKind : uint8_t { Elevation, VectorMap, StarCatalogue };

[[nodiscard]] const char *Name(DataKind kind) noexcept;

} // namespace outshine::Data
#endif

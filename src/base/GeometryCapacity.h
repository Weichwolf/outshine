#ifndef OUTSHINE_BASE_GEOMETRYCAPACITY_H
#define OUTSHINE_BASE_GEOMETRYCAPACITY_H
#include <cstddef>
#include <limits>

namespace outshine {
[[nodiscard]] constexpr bool CanAppendGeometryEntry(size_t count, size_t storageLimit) noexcept {
  return count < static_cast<size_t>(std::numeric_limits<int>::max()) && count < storageLimit;
}
}
#endif

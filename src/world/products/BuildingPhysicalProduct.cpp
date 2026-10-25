#include "BuildingPhysicalProduct.h"
#include "Capacity.h"
#include <cstddef>

namespace outshine::Ground {
size_t BuildingPhysicalProduct::HeapBytes() const noexcept {
  return CapacityBytes(Prints) + CapacityBytes(SeatSpreadM) + CapacityBytes(AcrossM) + ContactBytes;
}
}

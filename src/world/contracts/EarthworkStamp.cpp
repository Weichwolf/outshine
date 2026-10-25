#include "EarthworkStamp.h"
#include <cstddef>
#include <vector>

namespace outshine {
size_t EarthworkStamp::HeapBytes() const noexcept {
  size_t bytes = (RingEastNorthM.capacity() + SeamEastNorthM.capacity()) * sizeof(double);
  bytes += HoleRingsEastNorthM.capacity() * sizeof(std::vector<double>);
  for (const auto &hole : HoleRingsEastNorthM) { bytes += hole.capacity() * sizeof(double); }
  return bytes;
}

double EarthworkStamp::WantsAt(EastNorth at) const {
  const double east = at.EastM - AtE;
  const double north = at.NorthM - AtN;
  return PlateauM + SlopeE * east + SlopeN * north - 0.5 * (east * east + north * north) * SagInv;
}
}

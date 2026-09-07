#ifndef OUTSHINE_GENERATORS_BASE_CLUSTERID_H
#define OUTSHINE_GENERATORS_BASE_CLUSTERID_H

#include <cstdint>

namespace outshine::Generators {

enum class ClusterId : uint32_t {};

struct Scattered {
  double Em = 0.0, Nm = 0.0;
  double AslM = 0.0;
  float YawRad = 0.0f;
  float Scale = 1.0f;
};

} // namespace outshine::Generators
#endif

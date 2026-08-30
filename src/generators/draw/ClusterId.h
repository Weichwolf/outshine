#ifndef OUTSHINE_GENERATORS_DRAW_CLUSTERID_H
#define OUTSHINE_GENERATORS_DRAW_CLUSTERID_H

#include <cstdint>

namespace outshine::Generators {

enum class ClusterId : uint32_t {};

struct Instance {
  float Em = 0.0f, Nm = 0.0f;
  float AslM = 0.0f;
  float YawRad = 0.0f;
  float Scale = 1.0f;
};

} // namespace outshine::Generators
#endif

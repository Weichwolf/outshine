#ifndef CLUSTERID_H
#define CLUSTERID_H

#include <cstdint>

namespace outshine::Generators {

/* A prototype the client uploaded once at bring-up. A generator draws instances of one; growing it
 * is not a generator call. */
enum class ClusterId : uint32_t {};

struct Instance {
  float Em = 0.0f, Nm = 0.0f; /* metres east and north of the region anchor */
  float AslM = 0.0f;
  float YawRad = 0.0f;
  float ScaleM = 1.0f; /* the prototype is drawn at model scale times this */
};

} // namespace outshine::Generators
#endif

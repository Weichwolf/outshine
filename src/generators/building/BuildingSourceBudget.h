#ifndef OUTSHINE_GENERATORS_BUILDING_BUILDINGSOURCEBUDGET_H
#define OUTSHINE_GENERATORS_BUILDING_BUILDINGSOURCEBUDGET_H
#include <cstdint>

namespace outshine::Generators {
struct BuildingSourceBudget {
  uint64_t Samples = 0;
  uint64_t Sources = 0;
  uint64_t Faces = 0;
  uint64_t TriangleAttempts = 0;
};
}
#endif

#ifndef OUTSHINE_CONTENT_SCENE_PRISMGRID_H
#define OUTSHINE_CONTENT_SCENE_PRISMGRID_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace outshine {
struct alignas(8) PrismSpan {
  float Lower = 0;
  float Upper = 0;
};

static_assert(sizeof(PrismSpan) == 8 && alignof(PrismSpan) == 8);
static_assert(offsetof(PrismSpan, Lower) == 0 && offsetof(PrismSpan, Upper) == 4);

struct PrismPatch {
  uint64_t FirstSample = 0;
  uint32_t Columns = 0;
  uint32_t Rows = 0;
  float PitchX = 1;
  float PitchZ = 1;
  float FillFraction = 1;
};

struct PrismGrid {
  std::shared_ptr<const std::vector<PrismSpan>> Samples;
  std::vector<PrismPatch> Patches;
};

}
#endif

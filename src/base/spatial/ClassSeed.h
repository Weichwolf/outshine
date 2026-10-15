#ifndef OUTSHINE_BASE_SPATIAL_CLASSSEED_H
#define OUTSHINE_BASE_SPATIAL_CLASSSEED_H

#include "ClassSeedValues.h"
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace outshine {

inline constexpr size_t kClassSeedWords = static_cast<size_t>(kClassSeedWordCount);
inline constexpr uint32_t kClassSeedsPerCell = 32;
inline constexpr uint32_t kClassReferencesPerCell = kClassSeedsPerCell * 255u;

struct ClassSeed {
  uint32_t Labels;
  uint32_t FirstReference;
  uint32_t ReferenceCount;
  float HalfWidthM;
};

static_assert(std::is_standard_layout_v<ClassSeed>);
static_assert(std::is_trivially_copyable_v<ClassSeed>);
static_assert(sizeof(ClassSeed) == kClassSeedWords * sizeof(uint32_t));
static_assert(alignof(ClassSeed) == alignof(uint32_t));
static_assert(offsetof(ClassSeed, Labels) == 0);
static_assert(offsetof(ClassSeed, FirstReference) == sizeof(uint32_t));
static_assert(offsetof(ClassSeed, ReferenceCount) == 2 * sizeof(uint32_t));
static_assert(offsetof(ClassSeed, HalfWidthM) == 3 * sizeof(uint32_t));

}
#endif

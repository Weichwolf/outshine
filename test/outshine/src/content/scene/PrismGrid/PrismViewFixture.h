#ifndef OUTSHINE_TEST_PRISMVIEWFIXTURE_H
#define OUTSHINE_TEST_PRISMVIEWFIXTURE_H

#include "PrismGridViews.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace outshine::Test::PrismViewFixture {
inline PrismGrid Grid(uint32_t columns, uint32_t rows) {
  return {.Samples = std::make_shared<const std::vector<PrismSpan>>(
              static_cast<size_t>(columns) * rows, PrismSpan{.Lower = 0, .Upper = 1}),
          .Patches = {{.Columns = columns, .Rows = rows, .PitchX = 2, .PitchZ = 3}}};
}
}
#endif

#include "ModelLadder.h"
#include "Check.h"
#include <array>
#include <limits>

static_assert(outshine::ModelLadder::RelativeDeviation(0) == 1.0f / 4096);
static_assert(outshine::ModelLadder::RelativeDeviation(3) == 1.0f / 512);
static_assert(!outshine::ModelLadder::RelativeDeviation(4));
static_assert(!outshine::ModelLadder::RelativeDeviation(std::numeric_limits<size_t>::max()));

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr std::array<float, 4> expected{1.0f / 4096, 1.0f / 2048, 1.0f / 1024, 1.0f / 512};
  for (size_t rank = 0; rank < expected.size(); ++rank) {
    const auto deviation = ModelLadder::RelativeDeviation(rank);
    CHECK(deviation && *deviation == expected[rank],
          "detail budget matches independent powers of two");
  }
  for (size_t rank : {size_t{4}, size_t{5}, size_t{32}, std::numeric_limits<size_t>::max()}) {
    CHECK(!ModelLadder::RelativeDeviation(rank), "invalid rank refused before shift");
  }
  return Report();
}

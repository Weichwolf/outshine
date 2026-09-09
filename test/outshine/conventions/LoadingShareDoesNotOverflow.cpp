#include <Outshine.h>
#include "Check.h"
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr size_t most = std::numeric_limits<size_t>::max();
  CHECK(Loading{}.share() == 1.0, "no demand is complete");
  Loading progress;
  progress.GroundWanted = most;
  progress.VectorWanted = most;
  progress.GroundArrived = most;
  CHECK(progress.share() == 0.5, "half of two maximal counters does not wrap");
  progress.VectorArrived = most;
  CHECK(progress.share() == 1.0, "completed maximal counters remain complete");
  progress.GroundArrived = 0;
  progress.VectorArrived = 0;
  progress.VectorWanted = 1;
  CHECK(progress.share() == 0.0, "wrapped integer demand must not look empty");
  return Report();
}

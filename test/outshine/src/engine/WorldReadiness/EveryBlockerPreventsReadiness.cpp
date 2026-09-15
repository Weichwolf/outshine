#include "WorldReadiness.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  constexpr WorldReadiness complete;
  static_assert(complete.Ready());
  CHECK(complete.Describe().empty(), "complete snapshot reports no blocker");
  for (size_t stage = 0; stage < complete.Blockers.size(); ++stage) {
    WorldReadiness pending;
    pending.Blockers[stage] = "unfinished stage";
    CHECK(!pending.Ready(), "every individual stage prevents readiness");
    CHECK(pending.Describe() == "unfinished stage", "the incomplete stage is reported");
    CHECK(!pending.Ready(), "diagnostics do not mutate readiness");
  }
  WorldReadiness multiple;
  multiple.Blockers.front() = "first";
  multiple.Blockers.back() = "last";
  CHECK(multiple.Describe() == "first, last", "all blockers survive sparse stage reporting");
  CHECK(!multiple.Ready(), "multiple blockers prevent readiness");
  return Report();
}

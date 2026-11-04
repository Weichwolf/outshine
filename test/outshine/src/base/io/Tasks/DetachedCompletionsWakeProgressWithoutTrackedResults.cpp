#include "Tasks.h"
#include "Check.h"
#include <cstdint>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Tasks workers(1);
  const uint64_t detached = workers.CompletionEpoch();
  CHECK(workers.PostDetached([] {}), "a detached job is admitted");
  CHECK(workers.AwaitProgress(detached, 1), "detached completion wakes a progress observer");
  const uint64_t completed = workers.CompletionEpoch();
  CHECK(completed != detached && workers.AwaitProgress(detached, 1),
        "completion before waiting is preserved by the caller's snapshot");
  CHECK(!workers.AwaitProgress(completed, .001), "old progress cannot satisfy a fresh wait");
  CHECK(!workers.AwaitCompletion(.001), "detached progress creates no tracked completion");
  const auto tracked = workers.Post([] {});
  CHECK(workers.AwaitProgress(completed, 1) && workers.TakeCompletion(tracked),
        "tracked jobs report progress without consuming their owner's completion");
  CHECK(!workers.AwaitProgress(workers.CompletionEpoch(), 0), "a nonpositive wait never blocks");
  return Report();
}

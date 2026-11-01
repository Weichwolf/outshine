#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "Check.h"
#include "JobAdmission.h"
#include "JobAdmissionFixture.h"
#include <chrono>
#include <cstddef>
#include <memory>
#include <thread>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto admission = std::make_shared<JobAdmission>(1);
  BuildingQueueFixture first;
  BuildingQueueFixture second;
  if (!first.Open() || !second.Open()) { return Report(); }
  first.Queue.Opens(&first.Pool, first.MesherOwner, admission, outshine::Test::ContextAdmissions());
  second.Queue.Opens(
      &second.Pool, second.MesherOwner, admission, outshine::Test::ContextAdmissions());
  CHECK(first.Post() == 1 && admission->Active() == 1,
        "the first queue owns its shared admission across completed slices");
  const auto copies = second.Copies;
  CHECK(second.Post() == 0 && second.Copies == copies && second.Queue.Queued() == 0 &&
            second.Queue.AdmissionDeferred() == 1,
        "another queue cannot resolve height fields or construct work before admission");
  first.Queue.Clear();
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (admission->Active() != 0 && std::chrono::steady_clock::now() < deadline) {
    std::this_thread::yield();
  }
  CHECK(admission->Active() == 0, "the last payload owner returns the first queue's credit");
  size_t posted = 0;
  while (posted == 0 && std::chrono::steady_clock::now() < deadline) {
    posted = second.Post();
    if (posted == 0) { std::this_thread::yield(); }
  }
  CHECK(posted == 1 && second.Copies > copies && admission->Active() == 1,
        "the waiting queue advances unchanged once that same credit becomes available");
  second.Queue.Clear();
  return Report();
}

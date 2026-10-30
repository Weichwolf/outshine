#include "test/harness/engine/RoadPublicationContention.h"
#include "Check.h"
#include "ControlledRoadPageProducer.h"
#include "RoadPublicationFixture.h"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <memory>
#include <stop_token>
#include <string>
#include <thread>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::RoadPublication;

void CheckSharedCold() {
  Fixture fixture;
  CHECK(fixture.Open(), "the shared cold case uses the real native page cache");
  if (!fixture.Cache) { return; }
  std::atomic<size_t> cooks{0};
  auto input = std::make_shared<int>(7);
  const std::weak_ptr<int> source = input;
  auto producer = RoadProducer([input = std::move(input), &cooks](
                                   const Generators::RoadPageRequest &, const std::stop_token &) {
    ++cooks;
    return input ? Triangle() : Generators::RoadPageGeometry{};
  });
  RoadPageQueue a(fixture.Pool,
                  fixture.Cache,
                  {.PendingMost = 1, .PageBytesMost = kBytesMost},
                  fixture.Pipeline);
  RoadPageQueue b(fixture.Pool,
                  fixture.Cache,
                  {.PendingMost = 1, .PageBytesMost = kBytesMost},
                  fixture.Pipeline);
  const auto first = Request('a', 'c');
  const auto second = Request('a', 'd');
  fixture.Hold();
  CHECK(AdmitRoadPage(a, first, std::move(producer)) && AdmitRoadPage(b, second),
        "two request aliases share one admitted cold native cook");
  fixture.Barrier();
  CHECK(cooks.load() == 1 && source.expired() && fixture.Pipeline->Counts().Running == 0 &&
            fixture.Pipeline->Counts().Jobs == 1,
        "publication contention retains the cooked package and frees producer inputs and worker");
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(50);
  while (std::chrono::steady_clock::now() < until) {
    CHECK(a.Collect().empty() && b.Collect().empty() && a.Pending() == 1 && b.Pending() == 1,
          "both admitted consumers remain pending through native writer contention");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  fixture.Barrier();
  CHECK(cooks.load() == 1, "writer retries never repeat road generation");
  fixture.Release();
  const auto left = Await(a);
  const auto right = Await(b);
  const auto *one = Record(left);
  const auto *two = Record(right);
  CHECK(one && two && one->Package == two->Package && one->RequestKey == first.RequestKey &&
            two->RequestKey == second.RequestKey && cooks.load() == 1,
        "each consumer receives its binding to the same committed immutable package");
  auto independent = Pages::Open(fixture.Directory.string());
  CHECK(independent.has_value(),
        "a separate cache owner restores durable aliases without a producer");
  if (independent) {
    CheckGeometry(**independent, first);
    CheckGeometry(**independent, second);
  }
}

void CheckWarmCancel() {
  Fixture fixture;
  CHECK(fixture.Open(), "the warm alias case uses the real native page cache");
  if (!fixture.Cache) { return; }
  const auto producer = RoadProducer(
      [](const Generators::RoadPageRequest &, const std::stop_token &) { return Triangle(); });
  CHECK(fixture.Cache->Prepare(Request(), producer.get()).has_value(),
        "the warm product is committed before alias writer contention");
  RoadPageQueue a(fixture.Pool,
                  fixture.Cache,
                  {.PendingMost = 1, .PageBytesMost = kBytesMost},
                  fixture.Pipeline);
  RoadPageQueue b(fixture.Pool,
                  fixture.Cache,
                  {.PendingMost = 1, .PageBytesMost = kBytesMost},
                  fixture.Pipeline);
  const auto first = Request('a', 'e');
  const auto second = Request('a', 'f');
  fixture.Hold();
  CHECK(AdmitRoadPage(a, first) && AdmitRoadPage(b, second),
        "independent aliases request the same valid cached product");
  fixture.Barrier();
  CHECK(a.Collect().empty() && b.Collect().empty(),
        "the owner queues alias IO without doing SQLite work in Collect");
  fixture.Barrier();
  CHECK(a.Collect().empty() && b.Collect().empty(),
        "Busy request binding remains pending after the actual worker attempts");
  a.Cancel(Generators::RoadPageKey(first.Address));
  fixture.Barrier();
  const auto canceled = Await(a);
  CHECK(canceled.size() == 1 && !canceled.front().Result,
        "canceling an idle alias completes even while the writer remains held");
  CHECK(b.Collect().empty() && b.Pending() == 1,
        "canceling one alias preserves the other consumer and its bounded slot");
  fixture.Release();
  const auto surviving = Await(b);
  CHECK(Record(surviving) && Record(surviving)->RequestKey == second.RequestKey,
        "the surviving alias resumes and returns its own durable binding");
  const auto absent = fixture.Cache->LoadRequest(first.RequestKey, kBytesMost);
  CHECK(absent && !*absent, "the canceled alias does not publish after cancellation");
  CheckGeometry(*fixture.Cache, second);
}

void CheckPermanentProducer() {
  Fixture fixture;
  CHECK(fixture.Open(), "a permanent producer error uses its own native cache");
  if (!fixture.Cache) { return; }
  const auto calls = std::make_shared<std::atomic<size_t>>(0);
  const std::string message = "busy text belongs to a permanent geometry validation error";
  const auto producer =
      RoadProducer([calls, message](const Generators::RoadPageRequest &, const std::stop_token &)
                       -> std::expected<Generators::RoadPageGeometry, std::string> {
        ++*calls;
        return std::unexpected(message);
      });
  RoadPageQueue queue(fixture.Pool,
                      fixture.Cache,
                      {.PendingMost = 1, .PageBytesMost = kBytesMost},
                      fixture.Pipeline);
  CHECK(AdmitRoadPage(queue, Request(), producer),
        "the failing producer receives normal admission");
  const auto result = Await(queue);
  CHECK(
      result.size() == 1 && !result.front().Result && result.front().Result.error() == message &&
          calls->load() == 1 && queue.Pending() == 0,
      "only typed cache Busy retries; permanent text errors finish once with their original cause");
}
}

int main() {
  CheckSharedCold();
  CheckWarmCancel();
  CheckPermanentProducer();
  return outshine::Test::Report();
}

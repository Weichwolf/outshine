#include "PrismPreparationFixture.h"
#include "Check.h"
#include "Sha256.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <stop_token>
#include <string>
#include <thread>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Generators::Osm;
namespace Fixture = PrismPreparationFixture;
constexpr auto kDeadline = std::chrono::seconds(5);

struct Case {
  Fixture::Cache Directory;
  Tasks Pool{1};
  const std::string Request = Sha256Hex("prism contention/native source/terrain/selection/lod");
  std::unique_ptr<UrbanPrismPreparation> Job;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};
  size_t Captures = 0;
  std::atomic_uint32_t Generations{0};
  std::weak_ptr<const UrbanPrisms> Input;

  ~Case() {
    if (Job) { Job->Cancel(); }
    const auto barrier = Pool.Post([] {});
    if (barrier != Tasks::kNoTask) { Pool.Wait(barrier); }
    Job.reset();
  }

  bool Open(bool hold) {
    auto initialized = AssetCache::Open(Directory.Database());
    CHECK(initialized, "the real native asset schema is initialized before contention");
    if (!initialized) { return false; }
    sqlite3 *connection = nullptr;
    const int opened = sqlite3_open(Directory.Database().c_str(), &connection);
    Writer.reset(connection);
    CHECK(opened == SQLITE_OK, "an independent SQLite connection opens");
    if (opened != SQLITE_OK) { return false; }
    if (hold) {
      CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
            "a real writer transaction holds package publication admission");
    }
    Job = std::make_unique<UrbanPrismPreparation>(Pool, Directory.Database(), Request);
    return true;
  }

  UrbanPrismPreparation::Capture Capture() {
    return [this](const std::stop_token &) -> std::optional<UrbanPrismPreparation::Captured> {
      ++Captures;
      const auto input = std::make_shared<const UrbanPrisms>(Fixture::Product());
      Input = input;
      return AssetFactory{[this, input](size_t budget) {
        ++Generations;
        return PackageUrbanPrisms(*input, Request, {.BytesMost = budget});
      }};
    };
  }

  bool WaitHeld() {
    const auto deadline = std::chrono::steady_clock::now() + kDeadline;
    const auto capture = Capture();
    while (std::chrono::steady_clock::now() < deadline) {
      const auto value = Job->Advance(capture);
      CHECK(value && !*value,
            "a contended generated prism remains pending without a permanent error");
      if (!value || *value) { return false; }
      if (Job->Measurements().PublicationAttempts > 0 && !Job->InFlight()) { return true; }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    CHECK(false, "the actual preparation reaches a held prepared package");
    return false;
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases package admission");
  }
};

void Resume() {
  Case test;
  if (!test.Open(true) || !test.WaitHeld()) { return; }
  const auto bytes = test.Job->Measurements().PublicationBytes;
  CHECK(bytes > 0 && test.Job->HoldsAdmission() && test.Input.expired(),
        "pending admission retains encoded capacity and releases the captured source product");
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  while (std::chrono::steady_clock::now() < until) {
    const auto value = test.Job->Advance(test.Capture());
    CHECK(value && !*value, "temporary contention cannot become a visible prism product");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto costs = test.Job->Measurements();
  CHECK(
      costs.PublicationPreparations == 1 && costs.PublicationBytes == bytes &&
          costs.PublicationAttempts > 1 && costs.PublicationAttempts < 20 && test.Captures == 1 &&
          test.Generations == 1,
      "bounded readmission retries the owned package without recapture, generation or compression");
  bool progressed = false;
  const auto other = test.Pool.Post([&progressed] { progressed = true; });
  CHECK(other != Tasks::kNoTask, "unrelated work enters the same worker pool");
  if (other != Tasks::kNoTask) { test.Pool.Wait(other); }
  CHECK(progressed, "a contended publication does not sleep or spin on the worker");
  test.Release();
  const auto result = Fixture::Await(*test.Job, test.Capture());
  CHECK(result && *result && !test.Job->HoldsAdmission() &&
            test.Job->Measurements().PublicationBytes == 0 && test.Captures == 1 &&
            test.Generations == 1,
        "release completes the original prepared product and releases its admission and package");
  auto cache = AssetCache::Open(test.Directory.Database());
  const auto expected = EncodeUrbanPrisms(Fixture::Product());
  CHECK(cache && expected, "stored native bytes can be verified independently");
  if (!cache || !expected) { return; }
  const auto stored = (*cache)->LoadRequest(test.Request, kUrbanPrismBytesMost);
  CHECK(stored && *stored && std::ranges::equal((**stored).Bytes(), *expected),
        "the committed prism bytes exactly match the uncontended native input");
  CHECK(sqlite3_exec(test.Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "an independent writer also holds the fresh warm-reader case");
  UrbanPrismPreparation warm(test.Pool, test.Directory.Database(), test.Request);
  const auto hit = Fixture::Await(warm, test.Capture());
  CHECK(hit && *hit && test.Captures == 1 && test.Generations == 1 &&
            warm.Measurements().PublicationAttempts == 0 && warm.Measurements().Generations == 0,
        "a fresh warm owner restores under writer contention without capture or publication");
  test.Release();
}

void CollectOnlyAndCancel() {
  Case test;
  if (!test.Open(true) || !test.WaitHeld()) { return; }
  const auto attempts = test.Job->Measurements().PublicationAttempts;
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(30);
  while (std::chrono::steady_clock::now() < until) {
    const auto value = test.Job->Advance(test.Capture(), false);
    CHECK(value && !*value && !test.Job->InFlight() && test.Job->HoldsAdmission(),
          "collect-only retains admission without posting a due publication retry");
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  CHECK(test.Job->Measurements().PublicationAttempts == attempts && test.Captures == 1,
        "absence of work credit cannot enqueue or recapture a deferred package");
  test.Job->Cancel();
  CHECK(!test.Job->HoldsAdmission() && !test.Job->InFlight() &&
            test.Job->Measurements().PublicationBytes == 0 && !test.Job->Advance(),
        "canceling an idle deferred package releases its owner and admission immediately");
  test.Release();
  auto cache = AssetCache::Open(test.Directory.Database());
  CHECK(cache, "the canceled request can be inspected independently");
  if (!cache) { return; }
  const auto absent = (*cache)->FindRequest(test.Request);
  CHECK(absent && !*absent, "a canceled prepared package never publishes after admission returns");
}

void PermanentFailure() {
  Case test;
  if (!test.Open(false)) { return; }
  CHECK(sqlite3_exec(test.Writer.get(),
                     "CREATE TRIGGER reject_prisms BEFORE INSERT ON assets "
                     "WHEN NEW.kind = 'urban-prisms' "
                     "BEGIN SELECT RAISE(ABORT, 'fixture permanent storage failure'); END",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "a real permanent constraint rejects only the prepared prism package");
  const auto value = Fixture::Await(*test.Job, test.Capture());
  CHECK(!value && !test.Job->HoldsAdmission() && !test.Job->InFlight() &&
            test.Job->Measurements().PublicationBytes == 0 &&
            test.Job->Measurements().PublicationAttempts == 1 &&
            test.Job->Measurements().PublicationPreparations == 1 && test.Generations == 1,
        "permanent storage errors remain terminal after one attempt and release the pending owner");
}
}

int main() {
  Resume();
  CollectOnlyAndCancel();
  PermanentFailure();
  return Report();
}

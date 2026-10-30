#include "UrbanAssetFixture.h"
#include "Check.h"
#include <chrono>
#include <condition_variable>
#include <array>
#include <filesystem>
#include <mutex>
#include <limits>
#include <optional>
#include <string>
#include <system_error>

namespace {
using namespace UrbanAssetFixture;
using namespace outshine::Test;

void Missing(const std::filesystem::path &directory, Availability availability) {
  Fixture fixture(directory, availability);
  if (!fixture.Urban) { return; }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto result = Await(fixture, request);
  const auto wanted = availability == Availability::Refused ? PreparedUrbanAreas::State::Refused
                                                            : PreparedUrbanAreas::State::Absent;
  CHECK(result && result->Status == wanted && !result->Product,
        "missing or refused required input cannot publish a partial or empty urban product");
  const auto costs = fixture.Urban->Costs();
  CHECK(costs.Generations == 0 && costs.Writes == 0,
        "unavailable required sources never invoke the urban factory");
}

void BudgetFailure(const std::filesystem::path &directory, PreparedUrbanAreas::Budget budget) {
  Fixture fixture(directory, Availability::Ready, budget);
  if (!fixture.Urban) { return; }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  const auto result = Await(fixture, request);
  CHECK(!result && !result.error().empty(),
        "budget exhaustion is explicit rather than partial Ready");
  CHECK(fixture.Urban->Costs().Writes == 0,
        "failed generation never publishes an incomplete asset");
}

void Cancelled(const std::filesystem::path &directory) {
  Fixture fixture(directory, Availability::Ready);
  if (!fixture.Urban) { return; }
  const PreparedUrbanAreas::Request request(kOwner, MvtSchema::OpenMapTiles);
  CHECK(fixture.Urban->Acquire(request), "bounded lookup can be submitted");
  fixture.Urban->Cancel();
  CHECK(!fixture.Urban->Acquire(request), "cancellation rejects all later admission");
  fixture.Urban.reset();
  CHECK(fixture.Calls == 0 && fixture.Tiles->Costs().Misses == 0,
        "cancelled lookup never progresses into source acquisition");
}

void InvalidRequests(const std::filesystem::path &directory) {
  Fixture fixture(directory, Availability::Ready);
  if (!fixture.Urban) { return; }
  UrbanAreaRecipe recipe;
  recipe.ProjectedPitchM = std::numeric_limits<double>::quiet_NaN();
  const std::array requests{
      PreparedUrbanAreas::Request({.Zoom = -1}, MvtSchema::OpenMapTiles),
      PreparedUrbanAreas::Request({.Zoom = 0, .X = 1, .Y = 0}, MvtSchema::OpenMapTiles),
      PreparedUrbanAreas::Request(kOwner, MvtSchema::OpenMapTiles, recipe)};
  for (const auto &request : requests) {
    CHECK(!fixture.Urban->Acquire(request),
          "invalid tile or recipe fails before cache and source work");
  }
  PreparedUrbanAreas::Budget budget;
  budget.PendingMost = 0;
  CHECK(!PreparedUrbanAreas::Open(
            directory.string(), fixture.Session->Sources(), fixture.Compute, fixture.Tiles, budget),
        "zero work capacity is rejected explicitly");
  CHECK(fixture.Calls == 0 && fixture.Urban->Costs().Misses == 0,
        "invalid requests schedule no asset or source work");
}

void RetiredDemand(const std::filesystem::path &directory) {
  PreparedUrbanAreas::Budget budget;
  budget.PendingMost = 1;
  Fixture fixture(directory, Availability::Ready, budget);
  if (!fixture.Urban) { return; }
  std::mutex mutex;
  std::condition_variable ready;
  bool released = false;
  const auto blocker = fixture.Compute.Post([&] {
    std::unique_lock lock(mutex);
    ready.wait(lock, [&] { return released; });
  });
  const PreparedUrbanAreas::Request wanted({.Zoom = kOwner.Zoom, .X = kOwner.X + 1, .Y = kOwner.Y},
                                           MvtSchema::OpenMapTiles);
  {
    const PreparedUrbanAreas::Request abandoned(kOwner, MvtSchema::OpenMapTiles);
    CHECK(fixture.Urban->Acquire(abandoned), "one pending slot is admitted");
    const auto deferred = fixture.Urban->Acquire(wanted);
    CHECK(deferred && deferred->Status == PreparedUrbanAreas::State::Pending &&
              fixture.Urban->Costs().Deferred == 1,
          "full producer capacity defers later work instead of growing its task set");
  }
  {
    const std::scoped_lock lock(mutex);
    released = true;
  }
  ready.notify_all();
  fixture.Compute.Wait(blocker);
  const auto result = Await(fixture, wanted);
  CHECK(result && result->Status == PreparedUrbanAreas::State::Ready && result->Product,
        "expired demand releases its completed slot so live demand can progress");
  CHECK(fixture.Calls == 2 && fixture.Urban->Costs().Writes == 1,
        "abandoned metadata lookup performs no source work or generation");
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-urban-boundaries-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Missing(directory / "absent", Availability::Absent);
  Missing(directory / "parent-absent", Availability::ParentAbsent);
  Missing(directory / "refused", Availability::Refused);
  PreparedUrbanAreas::Budget budget;
  budget.CellsMost = 1;
  BudgetFailure(directory / "cells", budget);
  budget = {};
  budget.SourceBytesMost = 1;
  BudgetFailure(directory / "source-bytes", budget);
  budget = {};
  budget.GenerationTime = std::chrono::milliseconds::zero();
  BudgetFailure(directory / "deadline", budget);
  Cancelled(directory / "cancelled");
  InvalidRequests(directory / "invalid");
  RetiredDemand(directory / "retired");
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated boundary test products are removed");
  return Report();
}

#include "test/harness/engine/NativeBuildingSources.h"
#include "AssetCache.h"
#include "Check.h"
#include "PreparedBuildingAssets.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <sqlite3.h>
#include <string>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using namespace outshine::Test::BuildingSources;

struct Case {
  Fixture Env;
  std::unique_ptr<AssetCache> Database;
  std::unique_ptr<sqlite3, decltype(&sqlite3_close)> Writer{nullptr, sqlite3_close};
  std::shared_ptr<PreparedBuildingAssets> Cache;
  PreparedBuildingAssets::BasisRead Read;
  std::atomic_bool Stopping{false};
  std::string Key;
  std::string Request;
  AssetRecord Original;

  bool Open() {
    CHECK(Env.Open(), "the real vector/street/native fixture opens");
    if (!Env.Stack.BuildingAssets() || Env.Stack.Vectors() == nullptr) { return false; }
    Cache = Env.Stack.BuildingAssets();
    const auto generated = Env.Generate(17);
    CHECK(generated, "the existing producer seeds indexed native products and their request");
    if (!generated) { return false; }
    Key = *generated;
    const auto &tile = Env.Stack.Vectors()->Tiles().front();
    Request = Cache->RequestKey(
        {.Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)},
        Env.Stack.Pool().Shaped(),
        kSpan,
        tile.InputDigest);
    const auto path = Env.Directory.Path / "assets" / "assets.sqlite";
    auto opened = AssetCache::Open(path.string());
    CHECK(opened, "an independent native-cache owner opens");
    if (!opened) { return false; }
    Database = std::move(*opened);
    const auto found = Database->FindRequest(Request);
    CHECK(found && *found, "the native request identifies its current basis");
    if (!found || !*found) { return false; }
    Original = **found;
    auto damaged = Original;
    const std::array<uint8_t, 3> invalid{1, 2, 3};
    damaged.ByteCount = invalid.size();
    damaged.Package.clear();
    damaged.Bounds = {.Min = {{1, 2, 3}}, .Max = {{4, 5, 6}}};
    CHECK(Database->Publish(std::span(&damaged, 1), invalid),
          "only the derived basis is damaged; native index and cells remain");
    sqlite3 *writer = nullptr;
    const auto status = sqlite3_open(path.c_str(), &writer);
    Writer.reset(writer);
    CHECK(status == SQLITE_OK, "a real second writer controls publication admission");
    return status == SQLITE_OK;
  }

  bool Prepare() {
    const auto before = Cache->Costs();
    for (int step = 0; step < 16; ++step) {
      const auto result = Cache->ReadBasisRequest(Request, Read, Stopping);
      CHECK(result && !*result && Read.Pending(),
            "repair preserves its admission and original parent after invalid alias retirement");
      if (!result || *result || !Read.Pending()) { return false; }
      const auto costs = Cache->Costs();
      if (costs.BasisPreparations > before.BasisPreparations) {
        CHECK(costs.BasisPreparations == before.BasisPreparations + 1 &&
                  costs.BasisWrites == before.BasisWrites,
              "a bounded step prepares one basis without publishing it in the same step");
        const auto removed = Database->FindRequest(Request);
        CHECK(removed && !*removed, "invalid derived-basis retirement also removes its alias");
        return true;
      }
    }
    CHECK(false, "the owned request repair reaches its prepared package");
    return false;
  }

  void Hold() const {
    CHECK(sqlite3_exec(Writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "another connection holds the real writer boundary");
  }

  void Release() const {
    CHECK(sqlite3_exec(Writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the independent writer releases admission");
  }

  void Retry() {
    const auto before = Cache->Costs();
    for (int retry = 0; retry < 8; ++retry) {
      const auto result = Cache->ReadBasisRequest(Request, Read, Stopping);
      const auto *kind = result ? nullptr : std::get_if<StructureBakeErrorKind>(&result.error());
      CHECK(kind && *kind == StructureBakeErrorKind::ArtifactBusy && Read.Pending(),
            "writer contention retains the same owned request continuation");
    }
    const auto after = Cache->Costs();
    CHECK(after.ReadBytes == before.ReadBytes &&
              after.BasisPreparations == before.BasisPreparations &&
              after.BasisWrites == before.BasisWrites && after.Writes == before.Writes,
          "retries repeat neither index decoding, preparation nor native generation");
  }
};

void Repair() {
  Case test;
  if (!test.Open()) { return; }
  test.Hold();
  test.Retry();
  test.Release();
  if (!test.Prepare()) { return; }
  test.Hold();
  test.Retry();
  test.Release();
  const auto result = test.Cache->ReadBasisRequest(test.Request, test.Read, test.Stopping);
  CHECK(result && *result && !test.Read.Pending() && (**result).BaseKey == test.Key &&
            (**result).Bounds == test.Original.Bounds,
        "publication resumes the original native identity with repaired, current bounds");
  if (!result || !*result) { return; }
  const auto before = test.Cache->Costs();
  test.Hold();
  const auto hit = test.Cache->LoadBasisRequest(test.Request);
  CHECK(hit && *hit && (**hit).Product == (**result).Product &&
            test.Cache->Costs().BasisReadBytes == before.BasisReadBytes,
        "the valid resident request is read-only even under a real held writer");
  test.Release();
}

void Rebound() {
  Case test;
  if (!test.Open() || !test.Prepare()) { return; }
  test.Hold();
  test.Retry();
  test.Release();
  const auto current = test.Env.Generate(23);
  CHECK(current && *current != test.Key, "a current producer binds a different valid content key");
  if (!current) { return; }
  const auto before = test.Cache->Costs();
  const auto result = test.Cache->ReadBasisRequest(test.Request, test.Read, test.Stopping);
  const auto retained = test.Database->FindRequest(test.Request);
  const auto old = test.Database->Find(test.Original.Key);
  CHECK(result && *result && (**result).BaseKey == *current && !test.Read.Pending() && retained &&
            *retained && (**retained).Parent == *current && old && !*old &&
            test.Cache->Costs().BasisWrites == before.BasisWrites,
        "an old repair follows a concurrent valid rebound without publishing its stale package");
}

void Cancelled() {
  Case test;
  if (!test.Open() || !test.Prepare()) { return; }
  test.Hold();
  test.Retry();
  test.Stopping.store(true, std::memory_order_relaxed);
  const auto result = test.Cache->ReadBasisRequest(test.Request, test.Read, test.Stopping);
  const auto *kind = result ? nullptr : std::get_if<StructureBakeErrorKind>(&result.error());
  CHECK(kind && *kind == StructureBakeErrorKind::Cancelled && !test.Read.Pending(),
        "stop releases the retained request work without waiting for writer admission");
  test.Release();
  const auto retained = test.Database->FindRequest(test.Request);
  CHECK(retained && !*retained, "cancelled work does not recreate its retired alias");
}

void ActualQueue() {
  Case test;
  if (!test.Open()) { return; }
  test.Hold();
  bool pending = false;
  for (int retry = 0; retry < 8; ++retry) {
    const auto query =
        test.Env.Queue.PollNativeBasis(test.Env.Stack, test.Env.Stack.Footprints(), 0);
    CHECK(query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
          "the actual queue retains a busy request lookup in its admitted slot");
    pending = query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending;
    (void)test.Env.Worker.AwaitCompletion(0.001);
  }
  CHECK(pending, "the writer is exercised through the runtime lookup consumer");
  test.Release();
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto query =
        test.Env.Queue.PollNativeBasis(test.Env.Stack, test.Env.Stack.Footprints(), 0);
    CHECK(query, "bounded request repair remains a valid actual native lookup");
    if (!query) { return; }
    if (query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Ready) {
      CHECK(query->Source && query->Source->BaseKey == test.Key &&
                query->Source->Bounds == test.Original.Bounds,
            "the actual request consumer completes after its alias was retired and repaired");
      return;
    }
    CHECK(query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
          "alias retirement cannot turn an admitted repair into a permanent native miss");
    (void)test.Env.Worker.AwaitCompletion(0.001);
  }
  CHECK(false, "the actual request consumer finishes its bounded repair");
}
}

int main() {
  Repair();
  Rebound();
  Cancelled();
  ActualQueue();
  return Report();
}

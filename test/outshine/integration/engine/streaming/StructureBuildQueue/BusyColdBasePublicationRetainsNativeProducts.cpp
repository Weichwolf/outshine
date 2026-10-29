#include "test/harness/engine/NativeBuildingSources.h"
#include "AssetCache.h"
#include "BuildingInputs.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStructureCodec.h"
#include "StructureBuildTask.h"
#include "SourceSet.h"
#include "RawSourceCache.h"
#include <cstdint>
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <memory>
#include <optional>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <utility>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Generators;
using namespace outshine::Test;
using outshine::Test::BuildingSources::Fixture;
constexpr auto kDeadline = std::chrono::seconds(5);
using Writer = std::unique_ptr<sqlite3, decltype(&sqlite3_close)>;

class CountingMesh final : public BuildingMesh {
public:
  mutable size_t Calls = 0;

  std::expected<void, StructureMeshError>
  Mesh(const StructurePlan &plan, MeshScratch &scratch, Raised &into) const noexcept override {
    ++Calls;
    return BuildingMesh::Mesh(plan, scratch, into);
  }
};

struct Case {
  CountingMesh Mesher;
  Fixture Env;
  std::unique_ptr<RawTile> Raw;
  std::shared_ptr<const Ground::HeightField> Heights;
  Writer Database{nullptr, sqlite3_close};
  std::unique_ptr<StructureBuildTask> Work;
  std::string Key, Request;

  ~Case() {
    if (Work) {
      Work->RequestStop();
      Work->Join(Env.Worker);
    }
  }

  bool Open() {
    if (!Env.Open()) { return false; }
    const auto &vectors = *Env.Stack.Vectors();
    const auto &tile = vectors.Tiles().front();
    const Data::TileId address{
        .Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)};
    const auto &cache = Env.Stack.BuildingAssets();
    Key = cache->Key(address, 0, Env.Stack.Pool().Shaped(), 1000, tile.InputDigest);
    Request = cache->RequestKey(address, Env.Stack.Pool().Shaped(), 1000, tile.InputDigest);
    Raw = std::make_unique<RawTile>();
    Osm::PrepareBuildingInputs(
        vectors,
        Env.Stack.Footprints(),
        Env.Stack.Ways(),
        {.From = 0, .To = vectors.Features().size(), .Tile = 0, .Found = true},
        Env.Eye,
        std::nullopt,
        std::nullopt,
        *Raw);
    Raw->Eye = Env.Eye;
    Raw->Projection = {.FocalPx = 720};
    Ground::HeightField::Block block;
    block.At = {.Zoom = 0, .X = 0, .Y = 0};
    block.Raster = {.Side = 2, .Postings = 2};
    block.Nodes = {100, 100, 100, 100};
    Heights = Ground::HeightField::Of(0, {block});
    sqlite3 *raw = nullptr;
    const auto path = Env.Directory.Path / "assets/assets.sqlite";
    const int opened = sqlite3_open(path.c_str(), &raw);
    Database.reset(raw);
    CHECK(opened == SQLITE_OK && Heights && !Raw->Structures.empty(),
          "real source inputs, heights and an independent SQLite connection are available");
    return opened == SQLITE_OK && Heights && !Raw->Structures.empty();
  }

  void Hold() const {
    CHECK(sqlite3_exec(Database.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
          "an independent transaction holds publication admission");
  }

  void Release() const {
    CHECK(sqlite3_exec(Database.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the writer releases publication admission");
  }

  void Start() {
    Work = std::make_unique<StructureBuildTask>(0,
                                                std::move(Raw),
                                                Heights,
                                                std::make_unique<StructureBuildTask::Output>(),
                                                Mesher.Scratch());
    Work->UseNativeAssets(Env.Stack.BuildingAssets(), Key, Request);
    Work->UseInitialCells();
    Work->Start(Env.Worker, Mesher);
    Work->Join(Env.Worker);
  }

  void Pump() {
    if (Work->CanResume()) {
      Work->Resume(Env.Worker, Mesher);
      Work->Join(Env.Worker);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
};

bool Busy(const std::expected<PreparedBuildingAssets::Base, StructureBakeError> &result) {
  if (result) { return false; }
  const auto *kind = std::get_if<StructureBakeErrorKind>(&result.error());
  return kind != nullptr && *kind == StructureBakeErrorKind::ArtifactBusy;
}

bool HoldPublication(Case &test, PreparedBuildingAssets::BaseWrite &prepared) {
  const std::atomic_bool stopped{false};
  const auto &cache = test.Env.Stack.BuildingAssets();
  test.Hold();
  const auto first = cache->ResumeBase(prepared, stopped);
  const auto bytes = prepared.PublicationBytes();
  const auto packages = cache->Costs().BasePackagePreparations;
  CHECK(Busy(first) && bytes > 0,
        "surface, index and basis each retain typed Busy and owned bytes");
  for (unsigned attempt = 0; attempt < 3; ++attempt) {
    const auto next = cache->ResumeBase(prepared, stopped);
    CHECK(Busy(next) && prepared.PublicationBytes() == bytes &&
              cache->Costs().BasePreparations == 1 &&
              cache->Costs().BasePackagePreparations == packages,
          "admission retries retain the original raw product and exact prepared package");
  }
  const auto missing = cache->LoadBasisRequest(test.Request);
  CHECK(missing && !*missing,
        "the request alias is absent until the complete native basis commits");
  test.Release();
  const auto committed = cache->ResumeBase(prepared, stopped);
  CHECK(committed && !*committed,
        "one successful commit advances without premature product return");
  return committed.has_value();
}

void CheckPersistedRequest(Case &test, const Data::SourceSet &sources) {
  auto fresh = PreparedBuildingAssets::Open((test.Env.Directory.Path / "assets").string(), sources);
  CHECK(fresh, "a fresh native cache owner opens independently");
  if (!fresh) { return; }
  const auto alias = (*fresh)->LoadBasisRequest(test.Request);
  CHECK(alias && *alias && (**alias).BaseKey == test.Key,
        "a fresh owner independently restores the committed request alias");
}

void HoldEachPublication() {
  Case test;
  if (!test.Open()) {
    CHECK(false, "the cold native fixture opens");
    return;
  }
  const std::atomic_bool stopped{false};
  const auto &cache = test.Env.Stack.BuildingAssets();
  Data::RawSourceCache store(
      {.Directory = (test.Env.Directory.Path / "reference-sources").string()});
  const Data::SourceSet sources(store);
  auto baseline =
      PreparedBuildingAssets::Open((test.Env.Directory.Path / "reference").string(), sources);
  CHECK(baseline,
        "a separate uncontended native cache opens for the actual requested product keys");
  if (!baseline) { return; }
  const auto reference =
      (*baseline)->Generate(test.Key, *test.Raw, *test.Heights, stopped, test.Request);
  CHECK(reference && *reference, "the unchanged native inputs have a complete reference product");
  if (!reference || !*reference) { return; }
  const auto expected = EncodePreparedStructureIndex(**reference);
  auto prepared = cache->PrepareBase(test.Key, *test.Raw, *test.Heights, stopped, test.Request);
  CHECK(prepared, "the cold native product is generated once before publication");
  if (!prepared) { return; }
  const auto cells = (*baseline)->Costs().SurfaceWrites;
  CHECK(cells > 0, "the real input contains independently stored surface cells");
  for (unsigned phase = 0; phase < 3; ++phase) {
    if (!HoldPublication(test, *prepared)) { return; }
    if (phase == 0) {
      while (cache->Costs().SurfaceWrites < cells) {
        const auto next = cache->ResumeBase(*prepared, stopped);
        CHECK(next && !*next, "remaining surface cells retain separate native products");
        if (!next || *next) { return; }
      }
    }
  }
  const auto restored = cache->ResumeBase(*prepared, stopped);
  CHECK(restored && *restored && prepared->PublicationBytes() == 0,
        "only complete storage and validated restore release the retained cold product");
  if (!restored || !*restored) { return; }
  const auto actual = EncodePreparedStructureIndex(**restored);
  CHECK(expected && actual && *expected == *actual && cache->Costs().Writes == 1 &&
            cache->Costs().BasisWrites == 1 && cache->Costs().SurfaceWrites == cells,
        "resumed native index bytes and separate cell/basis products match the uncontended path");
  CheckPersistedRequest(test, sources);
}

void TaskContinuation(bool cancel) {
  Case test;
  if (!test.Open()) {
    CHECK(false, "the cold Task source fixture opens");
    return;
  }
  test.Hold();
  test.Start();
  const auto &cache = test.Env.Stack.BuildingAssets();
  const auto bytes = test.Work->Result().PublicationBytes;
  CHECK(test.Work->Result().Status && !test.Work->Result().Tile && bytes > 0,
        "cold generation remains a successful pending Task while its first cell commit is busy");
  if (!test.Work->Result().Status || bytes == 0) {
    test.Release();
    return;
  }
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(40);
  while (std::chrono::steady_clock::now() < until) { test.Pump(); }
  CHECK(cache->Costs().BasePreparations == 1 && cache->Costs().BasePackagePreparations == 1 &&
            test.Mesher.Calls == 0 && test.Work->Result().PublicationBytes == bytes &&
            test.Work->Result().PublicationAttempts > 1 &&
            test.Work->Result().PublicationAttempts < 20,
        "existing worker slices neither regenerate, repackage nor mesh during bounded admission "
        "retries");
  bool progressed = false;
  const auto other = test.Env.Worker.Post([&progressed] { progressed = true; });
  CHECK(other != Tasks::kNoTask, "unrelated work is admitted to the same worker");
  if (other != Tasks::kNoTask) { test.Env.Worker.Wait(other); }
  CHECK(progressed, "cold publication does not monopolize its worker");
  if (cancel) {
    test.Work->RequestStop();
    test.Work->Resume(test.Env.Worker, test.Mesher);
    test.Work->Join(test.Env.Worker);
  }
  test.Release();
  if (!cancel) {
    const auto deadline = std::chrono::steady_clock::now() + kDeadline;
    while (test.Work->Result().Status && !test.Work->Result().Tile &&
           std::chrono::steady_clock::now() < deadline) {
      test.Pump();
    }
  }
  CHECK(test.Work->Result().PublicationBytes == 0, "the pending package is released at completion");
  if (cancel) {
    CHECK(!test.Work->Result().Status && !test.Work->Result().Tile &&
              cache->Costs().SurfaceWrites == 0 && cache->Costs().Writes == 0 &&
              cache->Costs().BasisWrites == 0,
          "cancellation releases pending owners without committing any cold native product");
    return;
  }
  const auto &tile = test.Work->Result().Tile;
  CHECK(test.Work->Result().Status && tile, "admission release completes the original cold Task");
  if (tile) {
    CHECK(!tile->GeometryAssetKey.empty() && cache->Costs().BasePreparations == 1 &&
              cache->Costs().GeometryWrites == 1,
          "the resumed Task returns exactly one committed geometry without regenerating its base");
  }
}

void RejectPermanentFailure() {
  Case test;
  if (!test.Open()) {
    CHECK(false, "the cold Task fixture opens for permanent failure");
    return;
  }
  CHECK(sqlite3_exec(test.Database.get(),
                     "CREATE TRIGGER reject_cold_cell BEFORE INSERT ON assets "
                     "WHEN NEW.kind = 'building-forms' "
                     "BEGIN SELECT RAISE(ABORT, 'fixture permanent storage failure'); END",
                     nullptr,
                     nullptr,
                     nullptr) == SQLITE_OK,
        "a real permanent SQLite constraint rejects the first cold surface cell");
  test.Start();
  CHECK(!test.Work->Result().Status && !test.Work->Result().Tile &&
            test.Work->Result().PublicationBytes == 0 && test.Mesher.Calls == 0 &&
            test.Work->Result().PublicationAttempts == 1 &&
            test.Env.Stack.BuildingAssets()->Costs().BasePreparations == 1,
        "permanent cold publication errors release pending owners and remain terminal after one "
        "attempt");
}

}

int main() {
  HoldEachPublication();
  TaskContinuation(false);
  TaskContinuation(true);
  RejectPermanentFailure();
  return Report();
}

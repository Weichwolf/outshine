#include "BuildingSourceDemand.h"
#include "test/harness/engine/NativeBuildingReplay.h"
#include "BuiltinSourceProviders.h"
#include "test/outshine/src/engine/streaming/StructureBuildQueue/BuildingQueueFixture.h"
#include "BuildingInputs.h"
#include "HeightField.h"
#include "Check.h"
#include "Tasks.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <variant>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Test::BuildingReplay;
constexpr auto kDeadline = std::chrono::seconds(5);
constexpr double kSpan = 1000;

struct Fixture {
  BuildingQueueCache Directory;
  ReplaySourceTransport Wire;
  SilentSink Sink;
  Tasks Compute{1};
  Ground::SurfacePreparation Stack;
  Tasks Worker{1};
  StructureBuildQueue Queue;
  const LongitudeLatitude Eye{.LongitudeDeg = 0.01, .LatitudeDeg = 0.01};

  bool Open(std::filesystem::path directory = {}, bool warm = false) {
    if (directory.empty()) { directory = Directory.Path; }
    std::array<Data::SourceProvider, 2> providers{};
    providers[0].Kind = "terrain";
    providers[1].Kind = "vector";
    providers[1].Dataset = "fixture.native-replay";
    providers[1].Endpoint = "https://fixture.invalid/{z}/{x}/{y}.pbf";
    Data::ProviderRegistry registry;
    Generators::RegisterBuiltinSourceProviders(registry);
    if (!Stack.Open({.BuiltinData = "data",
                     .Sources = (directory / "sources").string(),
                     .Assets = (directory / "assets").string()},
                    providers,
                    registry,
                    Eye,
                    Wire,
                    Compute,
                    Sink,
                    nullptr,
                    1.0)) {
      return false;
    }
    if (!PrepareReplayInputs(Stack, Eye, warm, std::chrono::steady_clock::now() + kDeadline)) {
      return false;
    }
    Stack.Footprints().AnchorAt(TangentFrame::At(Eye).OriginEcef());
    Stack.Footprints().TilesSpan(kSpan);
    Queue.Opens(&Worker, nullptr);
    return true;
  }

  std::optional<std::string> Generate() {
    const auto &vectors = *Stack.Vectors();
    const auto &tile = vectors.Tiles().front();
    const Data::TileId address{
        .Zoom = tile.Z, .X = static_cast<uint32_t>(tile.X), .Y = static_cast<uint32_t>(tile.Y)};
    const auto &assets = Stack.BuildingAssets();
    auto key = assets->Key(address, 0, Stack.Pool().Shaped(), kSpan, tile.InputDigest);
    const auto request =
        assets->RequestKey(address, Stack.Pool().Shaped(), kSpan, tile.InputDigest);
    Generators::RawTile raw;
    Generators::Osm::PrepareBuildingInputs(
        vectors,
        Stack.Footprints(),
        Stack.Ways(),
        {.From = 0, .To = vectors.Features().size(), .Tile = 0, .Found = true},
        Eye,
        std::nullopt,
        std::nullopt,
        raw);
    Ground::HeightField::Block block;
    block.At = {.Zoom = 0, .X = 0, .Y = 0};
    block.Raster = {.Side = 2, .Postings = 2};
    block.Nodes = {100, 100, 100, 100};
    const auto heights = Ground::HeightField::Of(0, {block});
    const std::atomic_bool stopped{false};
    if (!heights) { return std::nullopt; }
    auto built = assets->Generate(key, raw, *heights, stopped, request);
    if (!built || !*built) { return std::nullopt; }
    return key;
  }
};

bool WaitFor(BuildingSourceDemand &demand, Fixture &fixture, BuildingSourceDemand::Phase phase) {
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  do {
    (void)demand.Advance(fixture.Queue, fixture.Stack, 1);
    if (demand.Entries().size() == 1 && demand.Entries()[0].Where == phase) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

void MissingThenPublished(Fixture &fixture) {
  BuildingSourceDemand demand;
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  const auto streets = fixture.Stack.Ways().LookedCount();
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Missing),
        "a pure native basis miss terminates without requesting geometry");
  const auto absent = fixture.Stack.BuildingAssets()->Costs();
  CHECK(fixture.Queue.Queued() == 0 && fixture.Stack.Ways().LookedCount() == streets &&
            absent.ReadBytes == before.ReadBytes &&
            absent.SurfaceReadBytes == before.SurfaceReadBytes &&
            absent.GeometryReadBytes == before.GeometryReadBytes,
        "metadata misses start no source forms, geometry, height pins or street ingestion");
  const auto generated = fixture.Generate();
  CHECK(generated.has_value(), "the ordinary native producer publishes actual MVT building inputs");
  if (!generated) { return; }
  const auto published = fixture.Stack.BuildingAssets()->Costs();
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Ready),
        "a newly published basis binding makes a previous metadata miss discoverable");
  const auto &entry = demand.Entries()[0];
  CHECK(entry.Value && entry.Value->BaseKey == *generated && !entry.Value->Bounds.Empty() &&
            entry.Value->Cells != 0,
        "the compact descriptor retains native content, ECEF bounds and occupied cells");
  const auto ready = fixture.Stack.BuildingAssets()->Costs();
  CHECK(ready.ReadBytes == published.ReadBytes &&
            ready.SurfaceReadBytes == published.SurfaceReadBytes &&
            ready.GeometryReadBytes == published.GeometryReadBytes,
        "discovering a ready source reads only the basis and request metadata");
  for (size_t turn = 0; turn < 10; ++turn) {
    (void)demand.Advance(fixture.Queue, fixture.Stack, 1);
  }
  CHECK(fixture.Stack.BuildingAssets()->Costs().BasisReadBytes == ready.BasisReadBytes,
        "unchanged World demand does not repeatedly decode the immutable basis");
  CHECK(demand.HeapBytes() > 0 && fixture.Queue.Queued() == 0,
        "World retains a compact source descriptor without live geometry jobs");
}

void UnavailableIsNotMissing(Fixture &fixture) {
  StructureBuildQueue unopened;
  BuildingSourceDemand demand;
  CHECK(demand.Advance(unopened, fixture.Stack, 1) && demand.Counts().Pending == 1 &&
            demand.Counts().Missing == 0 && unopened.Queued() == 0,
        "unconfigured execution remains unknown and does not become a cached miss");
}

void CoalescedAndBounded(Fixture &fixture) {
  std::atomic_bool released{false};
  const auto blocker = fixture.Worker.Post([&released] { released.wait(false); });
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  const auto first = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  const auto again = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  CHECK(first && again && first->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending &&
            again->RequestKey == first->RequestKey && fixture.Queue.Queued() == 1,
        "identical callers share one bounded worker lookup");
  CHECK(fixture.Stack.BuildingAssets()->Costs().BasisReadBytes == before.BasisReadBytes,
        "a held worker proves no caller-side basis read or decode");
  for (size_t at = 1; at <= StructureBuildQueue::kCandidateWindow; ++at) {
    fixture.Stack.Footprints().TilesSpan(kSpan + static_cast<double>(at));
    const auto query = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
    CHECK(query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
          "new demand remains pending while lookup admission is full");
  }
  CHECK(fixture.Queue.Queued() == StructureBuildQueue::kCandidateWindow,
        "native metadata work never exceeds the existing candidate window");
  released.store(true);
  released.notify_all();
  fixture.Worker.Wait(blocker);
  fixture.Queue.Clear();
  fixture.Stack.Footprints().TilesSpan(kSpan);
}

void CompactLeaseAndCamera(Fixture &fixture) {
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  auto query = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  while (query && query->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending &&
         std::chrono::steady_clock::now() < deadline) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    query = fixture.Queue.PollNativeBasis(fixture.Stack, fixture.Stack.Footprints(), 0);
  }
  CHECK(query && query->Source != nullptr, "the native source offers a borrowed complete basis");
  if (!query || query->Source == nullptr) { return; }
  const std::weak_ptr<const Generators::PreparedBuildingBasis> basis = query->Source->Product;
  BuildingSourceDemand demand;
  CHECK(demand.Advance(fixture.Queue, fixture.Stack, 1) && demand.Counts().Ready == 1,
        "World consumes the borrowed basis into its compact source descriptor");
  CHECK(basis.expired(), "the descriptor retains no full basis or source-coordinate owner");
  const auto before = fixture.Stack.BuildingAssets()->Costs();
  fixture.Stack.Footprints().SeenWith({.FocalPx = 1440});
  fixture.Stack.Footprints().ViewedFrom(
      TangentFrame::At({.LongitudeDeg = 0.011, .LatitudeDeg = 0.012}).OriginEcef());
  CHECK(!demand.Advance(fixture.Queue, fixture.Stack, 1) && demand.Counts().Ready == 1 &&
            fixture.Stack.BuildingAssets()->Costs().BasisReadBytes == before.BasisReadBytes,
        "camera and projected detail changes preserve immutable source demand");
  const auto shape = fixture.Stack.Pool().Shaped();
  auto changed = shape;
  ++changed.Seed;
  fixture.Stack.Pool().Shapes(changed);
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Missing) &&
            !demand.Entries()[0].Value,
        "changed source shape cannot retain the previous source descriptor");
  fixture.Stack.Pool().Shapes(shape);
  CHECK(WaitFor(demand, fixture, BuildingSourceDemand::Phase::Ready),
        "restoring exact source inputs restores their pinned asset binding");
}

void FreshWarmAndStoreScope(Fixture &original) {
  Fixture warm;
  CHECK(warm.Open(original.Directory.Path, true), "a fresh World opens cached native inputs");
  if (!warm.Stack.Opened() || warm.Stack.Vectors() == nullptr) { return; }
  BuildingSourceDemand demand;
  const auto before = warm.Stack.BuildingAssets()->Costs();
  CHECK(WaitFor(demand, warm, BuildingSourceDemand::Phase::Ready),
        "fresh World metadata does not depend on a prior geometry landing or street plan");
  const auto after = warm.Stack.BuildingAssets()->Costs();
  CHECK(warm.Stack.Ways().LookedCount() == 0 && after.ReadBytes == before.ReadBytes &&
            after.SurfaceReadBytes == before.SurfaceReadBytes &&
            after.GeometryReadBytes == before.GeometryReadBytes,
        "cached World source discovery never enters the geometry or street fallback");
  const auto foreign = warm.Queue.PollNativeBasis(original.Stack, original.Stack.Footprints(), 0);
  const auto *failure =
      foreign ? nullptr : std::get_if<Generators::StructureBakeErrorKind>(&foreign.error());
  CHECK(failure && *failure == Generators::StructureBakeErrorKind::ArtifactInvalidProduct,
        "a queue cannot return another asset store's request binding");
  warm.Queue.Clear();
  const auto rebound = warm.Queue.PollNativeBasis(original.Stack, original.Stack.Footprints(), 0);
  CHECK(rebound && rebound->Where == StructureBuildQueue::NativeBasisQuery::Phase::Pending,
        "clearing a queue permits a new store scope and a fresh bounded lookup");
  warm.Queue.Clear();
  warm.Stack.Close();
  CHECK(demand.Advance(warm.Queue, warm.Stack, 1) && demand.Entries().empty(),
        "unavailable current inputs invalidate previously ready source descriptions");
}
}

int main() {
  Fixture fixture;
  CHECK(fixture.Open(), "real native vector and asset services open");
  if (!fixture.Stack.Opened() || fixture.Stack.Vectors() == nullptr) { return Report(); }
  UnavailableIsNotMissing(fixture);
  MissingThenPublished(fixture);
  CoalescedAndBounded(fixture);
  CompactLeaseAndCamera(fixture);
  FreshWarmAndStoreScope(fixture);
  return Report();
}

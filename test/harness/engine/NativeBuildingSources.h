#ifndef OUTSHINE_TEST_ENGINE_NATIVEBUILDINGSOURCES_H
#define OUTSHINE_TEST_ENGINE_NATIVEBUILDINGSOURCES_H
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

namespace outshine::Test::BuildingSources {
using namespace outshine::Test::BuildingReplay;
inline constexpr auto kDeadline = std::chrono::seconds(5);
inline constexpr double kSpan = 1000;

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

inline bool
WaitFor(BuildingSourceDemand &demand, Fixture &fixture, BuildingSourceDemand::Phase phase) {
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  do {
    (void)demand.Advance(fixture.Queue, fixture.Stack, 1);
    if (demand.Entries().size() == 1 && demand.Entries()[0].Where == phase) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  } while (std::chrono::steady_clock::now() < deadline);
  return false;
}

}
#endif

#include "ShippedProviders.h"
#include "BuildingMesh.h"
#include "Check.h"
#include "SurfacePreparation.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "TerrainRevisionIndex.h"
#include "TangentFrame.h"
#include "MvtWireFixture.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <print>
#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace {
outshine::Test::Mvt::Bytes BuildingTile(bool twoCells) {
  using outshine::Test::Mvt::Append;
  using outshine::Test::Mvt::Bytes;
  Bytes layer{0x0a, 9, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g', 's', 0x78, 2, 0x28, 64};
  Append(layer, 0x1a, Bytes{'k', 'i', 'n', 'd'});
  Append(layer, 0x22, Bytes{0x0a, 8, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g'});
  Bytes feature{0x08, 1, 0x18, 3};
  Append(feature, 0x12, Bytes{0, 0});
  Append(feature, 0x22, Bytes{9, 62, 62, 26, 8, 0, 0, 8, 7, 0, 15});
  Append(layer, 0x12, feature);
  if (twoCells) {
    Bytes second{0x08, 3, 0x18, 3};
    Append(second, 0x12, Bytes{0, 0});
    Append(second, 0x22, Bytes{9, 20, 20, 26, 8, 0, 0, 8, 7, 0, 15});
    Append(layer, 0x12, second);
  }
  Bytes tile;
  Append(tile, 0x1a, layer);
  Bytes streetLayer{0x0a, 7, 's', 't', 'r', 'e', 'e', 't', 's', 0x78, 2, 0x28, 64};
  Append(streetLayer, 0x1a, Bytes{'k', 'i', 'n', 'd'});
  Append(streetLayer, 0x22, Bytes{0x0a, 11, 'r', 'e', 's', 'i', 'd', 'e', 'n', 't', 'i', 'a', 'l'});
  Bytes street{0x08, 2, 0x18, 2};
  Append(street, 0x12, Bytes{0, 0});
  Append(street, 0x22, Bytes{9, 0, 0, 10, 120, 0});
  Append(streetLayer, 0x12, street);
  Append(tile, 0x1a, streetLayer);
  return tile;
}

class ReplaySourceTransport final : public outshine::Data::SourceTransport {
public:
  bool TwoCells = false;

  outshine::Data::FetchStart Begin(const std::string & /*uri*/) override {
    return static_cast<outshine::Data::Ticket>(1);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket /*ticket*/) override {
    return outshine::Data::Wire::Answered(200, BuildingTile(TwoCells));
  }

  void Cancel(outshine::Data::Ticket /*ticket*/) override {}
};

class SilentSink final : public outshine::Sink {
public:
  void Number(const char * /*name*/, double /*value*/, const char * /*unit*/) override {}

  void Claim(bool /*passed*/, const char * /*claim*/) override {}

  void Near(double /*got*/,
            double /*want*/,
            double /*within*/,
            const char * /*unit*/,
            const char * /*why*/) override {}

  void Say(const std::string & /*uri*/) override {}
};

using namespace outshine;
using namespace outshine::Ground;
using namespace outshine::Test;

bool PrepareReplayInputs(SurfacePreparation &stack,
                         LongitudeLatitude eye,
                         bool warm,
                         std::chrono::steady_clock::time_point deadline) {
  const auto inputsReady = [&] {
    return warm ? stack.Vectors() != nullptr && stack.Vectors()->SettledWithin(0)
                : stack.Ingested();
  };
  while (!inputsReady() && std::chrono::steady_clock::now() < deadline) {
    CHECK(stack.AdvanceAt(eye, {.IngestTilesMost = warm ? 0u : 1u, .VectorRing = 0}).has_value(),
          "the real vector snapshot and semantic inputs advance");
    (void)stack.AwaitProgress(0.001);
  }
  CHECK(inputsReady() && stack.Vectors() != nullptr && stack.Vectors()->Tiles().size() == 1,
        "one complete vector tile supplies the native product");
  if (!inputsReady() || stack.Vectors() == nullptr || stack.Vectors()->Tiles().size() != 1) {
    return false;
  }
  CHECK(inputsReady(), "the required source or native admission metadata is available");
  CHECK(!warm || stack.Ways().LookedCount() == 0,
        "warm replay starts without preparing street inputs");
  CHECK(warm || !stack.Ways().Ways().empty(),
        "the cold asset binds an actual street corridor instead of an empty digest");
  CHECK(!warm || (stack.PreparedVectorCosts().Hits > 0 && stack.PreparedVectorCosts().Writes == 0 &&
                  stack.Vectors()->TotalBuildMetrics().ParseMs == 0),
        "fresh warm replay loads normalized native tiles before source decode");
  return true;
}

struct ReplayTerrain {
  ReplayTerrain(bool warm, std::unique_ptr<TerrainRevisionIndex> revisions)
      : Warm(warm), Revisions(std::move(revisions)) {}

  bool Warm = false;
  std::unique_ptr<TerrainRevisionIndex> Revisions;
  std::map<std::tuple<int, uint32_t, uint32_t>, std::shared_ptr<TerrainField>> Fields;
  size_t Pins = 0;
  size_t RepeatedPins = 0;
  bool DeliveredThisPump = false;

  bool Pin(Data::TileId tile, HeightField::Block &into) {
    ++Pins;
    if (Warm) { return false; }
    const auto key = std::tuple{tile.Zoom, tile.X, tile.Y};
    if (Fields.contains(key)) {
      ++RepeatedPins;
      return false;
    }
    if (DeliveredThisPump) { return false; }
    DeliveredThisPump = true;
    auto &field = Fields[key];
    if (!field) {
      const auto stamp = (*Revisions).IssueDeliveryStamp(tile);
      if (!stamp) { return false; }
      field = std::make_shared<TerrainField>(3, 3);
      std::fill_n(field->Data(), 9, 100.0f);
      field->AddSource({.Kind = Data::DataKind::Elevation,
                        .Tile = tile,
                        .SourceId = "test-dem",
                        .Revision = "one"});
      field->SetCertificate(TerrainCertificate::FromDelivery(tile, *stamp, 11));
    }
    return HeightField::SharesField(field, tile, into);
  }
};

void CheckPartialLanding(const StructureBuildQueue::Landing &landing,
                         const Generators::Osm::BuildingField &prints,
                         const auto &initialContacts) {
  const auto contacts = prints.PinProducts();
  const bool unchanged =
      std::ranges::equal(contacts, initialContacts, [](const auto &a, const auto &b) {
        return a.Physical == b.Physical && a.Coordinates == b.Coordinates &&
               a.FirstFootprint == b.FirstFootprint;
      });
  CHECK(landing.SourceComplete || unchanged,
        "partial children do not repeatedly reserve or copy the accepted contact buffer");
  CHECK(landing.InitialCells != 0 && landing.Baked->RequestedCell &&
            landing.Baked->Prints.size() == 1 && (prints.FootprintCount() == 0) &&
            landing.Footprints.has_value() == landing.SourceComplete,
        "children land independently while source contacts remain unaccepted until complete");
}

bool LandReplay(SurfacePreparation &stack,
                StructureBuildQueue &queue,
                LongitudeLatitude eye,
                const StructureBuildQueue::HeightSource &source,
                ReplayTerrain &terrain,
                bool initialCells,
                std::chrono::steady_clock::time_point deadline) {
  auto &prints = stack.Footprints();
  const auto initialContacts = prints.PinProducts();
  bool landed = false;
  const auto purpose = initialCells ? StructureBuildQueue::BuildPurpose::SourceGeometry
                                    : StructureBuildQueue::BuildPurpose::ViewDetail;
  const size_t coordinateCount = initialCells ? 20 : 12;
  while (!landed && std::chrono::steady_clock::now() < deadline) {
    terrain.DeliveredThisPump = false;
    (void)queue.Posts(stack,
                      prints,
                      eye,
                      source,
                      1,
                      StructureBuildQueue::HeightRequirement::FineOnly,
                      std::nullopt,
                      purpose);
    auto ready = queue.NextLandings(stack,
                                    prints,
                                    eye,
                                    source,
                                    1,
                                    StructureBuildQueue::HeightRequirement::FineOnly,
                                    std::nullopt,
                                    purpose);
    CHECK(ready.has_value(), "native replay completes without a geometry error");
    if (!ready) { break; }
    if (!ready->empty()) {
      CHECK(ready->front().Baked->Coordinates &&
                ready->front().Baked->Coordinates->Points().size() == coordinateCount,
            "native geometry owns its complete ground-contact ring before publication");
      if (initialCells) { CheckPartialLanding(ready->front(), prints, initialContacts); }
      landed = ready->front().SourceComplete;
      queue.CommitsLandings(prints, *ready);
    } else {
      (void)queue.AwaitSlice(0.001);
      if (queue.Queued() == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    }
  }
  return landed;
}

void CheckPublication(SurfacePreparation &stack,
                      const StructureBuildQueue &queue,
                      const ReplayTerrain &terrain,
                      bool landed,
                      bool initialCells) {
  const bool warm = terrain.Warm;
  const size_t coordinateCount = initialCells ? 20 : 12;
  const size_t sourceCount = initialCells ? 2 : 1;
  auto &prints = stack.Footprints();
  CHECK(landed && prints.FootprintCount() == sourceCount, "the complete footprints land once");
  CHECK(warm || terrain.Fields.size() > 1,
        "cold input crosses multiple height fields delivered on separate pumps");
  CHECK(terrain.RepeatedPins == 0,
        "partial source delivery pins each field until the complete building input exists");
  if (!landed) {
    std::println("REPLAY warm={} features={} rings={} pins={} fields={} posted={} queued={} "
                 "deferred={}",
                 static_cast<int>(warm),
                 stack.Vectors()->Features().size(),
                 stack.Vectors()->Rings().size(),
                 terrain.Pins,
                 terrain.Fields.size(),
                 queue.Posted(),
                 queue.Queued(),
                 queue.Deferred());
  }
  const auto *input = prints.InputOfTile(0);
  CHECK(input && input->Coordinates && input->Coordinates->Points().size() == coordinateCount,
        "publication preserves native coordinates instead of moving an empty source buffer");
  CHECK(!warm || (terrain.Pins == 0 && stack.BuildingAssets()->Costs().GeometryHits == sourceCount),
        "fresh warm replay loads ready geometry without touching the height provider");
  const auto costs = stack.BuildingAssets()->Costs();
  CHECK(!warm || (costs.BasisHits == 1 && costs.Hits == 0 && costs.ReadBytes == 0),
        "fresh warm queue uses the owned basis without decoding the full building plans");
  CHECK(!warm || (input && input->Bake.PreparedAssetKey.has_value() &&
                  input->SourceKey == StructureBuildQueue::QualifiedSourceKey(prints, 0)),
        "native content identity remains the accepted dependency without source revalidation");
  CHECK(!warm || stack.Ways().LookedCount() == 0,
        "native replay leaves unprepared street inputs untouched");
}

void Replay(const std::filesystem::path &directory,
            bool warm,
            bool obsoleteLookups = false,
            bool initialCells = false) {
  const LongitudeLatitude eye{.LongitudeDeg = 0.01, .LatitudeDeg = 0.01};
  std::array<Data::SourceProvider, 2> providers{};
  providers[0].Kind = "terrain";
  providers[1].Kind = "vector";
  providers[1].Dataset = "fixture.native-replay";
  providers[1].Endpoint = "https://fixture.invalid/{z}/{x}/{y}.pbf";
  ReplaySourceTransport wire;
  wire.TwoCells = initialCells;
  SilentSink sink;
  Tasks compute(1);
  SurfacePreparation stack;
  Data::ProviderRegistry registry;
  Generators::RegisterShippedProviders(registry);
  CHECK(stack.Open({.BuiltinData = "data",
                    .Sources = (directory / "sources").string(),
                    .Assets = (directory / "assets").string()},
                   providers,
                   registry,
                   eye,
                   wire,
                   compute,
                   sink,
                   nullptr,
                   1.0),
        "native replay opens the real vector and asset services");
  if (!stack.Opened()) { return; }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  if (!PrepareReplayInputs(stack, eye, warm, deadline)) { return; }
  auto &prints = stack.Footprints();
  prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
  prints.TilesSpan(1000.0);
  prints.SeenWith({.FocalPx = 720.0});
  prints.BeginRefinement();
  auto revisions = TerrainRevisionIndex::Create(32);
  CHECK(revisions.has_value(), "terrain lineage storage is available");
  if (!revisions) { return; }
  ReplayTerrain terrain(warm, std::move(*revisions));
  const StructureBuildQueue::HeightSource source{
      .Sample = {},
      .PinField = [&terrain](Data::TileId tile,
                             HeightField::Block &into) { return terrain.Pin(tile, into); },
      .Revision = {.Value = 11}};
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&compute, &mesher);
  if (obsoleteLookups) {
    for (size_t version = 1; version <= 4; ++version) {
      prints.TilesSpan(1000.0 + static_cast<double>(version));
      CHECK(queue.Posts(stack, prints, eye, source, 1) == 0,
            "each obsolete request starts only an asynchronous native lookup");
      compute.Wait(compute.Post([] {}));
    }
    CHECK(queue.Queued() == 4, "completed obsolete lookups occupy the entire candidate window");
    prints.TilesSpan(1000.0);
  }
  const bool landed = LandReplay(stack, queue, eye, source, terrain, initialCells, deadline);
  CheckPublication(stack, queue, terrain, landed, initialCells);
  queue.Clear();
}
}

int main() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-native-contact-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  Replay(directory, false);
  Replay(directory, true);
  Replay(directory, true, true);
  const auto cells = directory / "initial-cells";
  Replay(cells, false, false, true);
  Replay(cells, true, false, true);
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated source and asset products are removed");
  return outshine::Test::Report();
}

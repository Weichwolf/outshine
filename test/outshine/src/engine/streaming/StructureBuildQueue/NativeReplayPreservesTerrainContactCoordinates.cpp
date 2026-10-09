#include "BuildingMesh.h"
#include "Check.h"
#include "SurfacePreparation.h"
#include "Sink.h"
#include "StructureBuildQueue.h"
#include "TerrainRevisionIndex.h"
#include "TangentFrame.h"
#include "test/outshine/src/generators/osm/MvtLayer/WireFixture.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <map>
#include <string>
#include <system_error>
#include <thread>
#include <tuple>
#include <vector>

namespace {
outshine::Test::Mvt::Bytes BuildingTile() {
  using outshine::Test::Mvt::Append;
  using outshine::Test::Mvt::Bytes;
  Bytes layer{0x0a, 9, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g', 's', 0x78, 2, 0x28, 64};
  Append(layer, 0x1a, Bytes{'k', 'i', 'n', 'd'});
  Append(layer, 0x22, Bytes{0x0a, 8, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g'});
  Bytes feature{0x08, 1, 0x18, 3};
  Append(feature, 0x12, Bytes{0, 0});
  Append(feature, 0x22, Bytes{9, 64, 64, 26, 8, 0, 0, 8, 7, 0, 15});
  Append(layer, 0x12, feature);
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

class Transport final : public outshine::Data::Transport {
public:
  outshine::Data::FetchStart Begin(const std::string &) override {
    return static_cast<outshine::Data::Ticket>(1);
  }

  outshine::Data::Wire Collect(outshine::Data::Ticket) override {
    return outshine::Data::Wire::Answered(200, BuildingTile());
  }

  void Cancel(outshine::Data::Ticket) override {}
};

class SilentSink final : public outshine::Sink {
public:
  void Number(const char *, double, const char *) override {}

  void Claim(bool, const char *) override {}

  void Near(double, double, double, const char *, const char *) override {}

  void Say(const std::string &) override {}
};

void Replay(const std::filesystem::path &directory, bool warm) {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const LongitudeLatitude eye{.LongitudeDeg = 0.01, .LatitudeDeg = 0.01};
  const std::array providers{
      Data::SourceProvider{.Kind = "terrain"},
      Data::SourceProvider{.Kind = "vector",
                           .Dataset = "fixture.native-replay",
                           .Endpoint = "https://fixture.invalid/{z}/{x}/{y}.pbf"}};
  Transport wire;
  SilentSink sink;
  Tasks compute(1);
  SurfacePreparation stack;
  CHECK(stack.Open({.Shipped = "src/assets",
                    .Cache = (directory / "sources").string(),
                    .AssetCache = (directory / "assets").string()},
                   providers,
                   eye,
                   wire,
                   compute,
                   sink,
                   nullptr,
                   1.0),
        "native replay opens the real vector and asset services");
  if (!stack.Opened()) { return; }
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  const auto inputsReady = [&] {
    return warm ? stack.Vectors() && stack.Vectors()->SettledWithin(0) : stack.Ingested();
  };
  while (!inputsReady() && std::chrono::steady_clock::now() < deadline) {
    CHECK(stack.AdvanceAt(eye, {.IngestTilesMost = warm ? 0u : 1u, .VectorRing = 0}).has_value(),
          "the real vector snapshot and semantic inputs advance");
    (void)stack.AwaitProgress(0.001);
  }
  CHECK(inputsReady() && stack.Vectors() && stack.Vectors()->Tiles().size() == 1,
        "one complete vector tile supplies the native product");
  if (!inputsReady() || !stack.Vectors() || stack.Vectors()->Tiles().size() != 1) { return; }
  CHECK(inputsReady(), "the required source or native admission metadata is available");
  CHECK(!warm || stack.Ways().LookedCount() == 0,
        "warm replay starts without preparing street inputs");
  CHECK(warm || !stack.Ways().Ways().empty(),
        "the cold asset binds an actual street corridor instead of an empty digest");
  CHECK(!warm || (stack.PreparedVectorCosts().Hits > 0 && stack.PreparedVectorCosts().Writes == 0 &&
                  stack.Vectors()->TotalBuildMetrics().ParseMs == 0),
        "fresh warm replay loads normalized native tiles before source decode");
  auto &prints = stack.Footprints();
  prints.AnchorAt(TangentFrame::At(eye).OriginEcef());
  prints.TilesSpan(1000.0);
  prints.SeenWith({.FocalPx = 720.0});
  prints.BeginRefinement();
  auto revisions = TerrainRevisionIndex::Create(32);
  CHECK(revisions.has_value(), "terrain lineage storage is available");
  if (!revisions) { return; }
  std::map<std::tuple<int, uint32_t, uint32_t>, std::shared_ptr<TerrainField>> fields;
  size_t pins = 0;
  StructureBuildQueue::HeightSource source{
      .Sample = {},
      .PinField =
          [&](Data::TileId tile, HeightField::Block &into) {
            ++pins;
            if (warm) { return false; }
            auto &field = fields[{tile.Zoom, tile.X, tile.Y}];
            if (!field) {
              const auto stamp = (**revisions).IssueDeliveryStamp(tile);
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
          },
      .ResidentField = {},
      .Revision = {.Value = 11}};
  Generators::BuildingMesh mesher;
  StructureBuildQueue queue;
  queue.Opens(&compute, &mesher);
  bool landed = false;
  while (!landed && std::chrono::steady_clock::now() < deadline) {
    (void)queue.Posts(
        stack, prints, eye, source, 1, StructureBuildQueue::HeightRequirement::FineOnly);
    auto ready = queue.NextLandings(
        stack, prints, eye, source, 1, StructureBuildQueue::HeightRequirement::FineOnly);
    CHECK(ready.has_value(), "native replay completes without a geometry error");
    if (!ready) { break; }
    if (!ready->empty()) {
      CHECK(ready->front().Baked->Coordinates &&
                ready->front().Baked->Coordinates->Points.size() == 12,
            "native geometry owns its complete ground-contact ring before publication");
      queue.CommitsLandings(prints, *ready);
      landed = true;
    } else {
      (void)queue.AwaitSlice(0.001);
      if (queue.Queued() == 0) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    }
  }
  CHECK(landed && prints.Footprints().size() == 1, "the complete footprint lands");
  if (!landed) {
    std::printf("REPLAY warm=%d features=%zu rings=%zu pins=%zu fields=%zu posted=%zu queued=%zu "
                "deferred=%zu\n",
                warm,
                stack.Vectors()->Features().size(),
                stack.Vectors()->Rings().size(),
                pins,
                fields.size(),
                queue.Posted(),
                queue.Queued(),
                queue.Deferred());
  }
  const auto *input = prints.InputOfTile(0);
  CHECK(input && input->Coordinates && input->Coordinates->Points.size() == 12,
        "publication preserves native coordinates instead of moving an empty source buffer");
  CHECK(!warm || (pins == 0 && stack.BuildingAssets()->Costs().GeometryHits == 1),
        "fresh warm replay loads ready geometry without touching the height provider");
  const auto costs = stack.BuildingAssets()->Costs();
  CHECK(!warm || (costs.BasisHits == 1 && costs.Hits == 0 && costs.ReadBytes == 0),
        "fresh warm queue uses the owned basis without decoding the full building plans");
  CHECK(!warm || (input && input->Bake.PreparedAssetKey.has_value() &&
                  input->SourceKey == StructureBuildQueue::QualifiedSourceKey(prints, 0)),
        "native content identity remains the accepted dependency without source revalidation");
  CHECK(!warm || stack.Ways().LookedCount() == 0,
        "native replay leaves unprepared street inputs untouched");
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
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  CHECK(!error, "isolated source and asset products are removed");
  return outshine::Test::Report();
}

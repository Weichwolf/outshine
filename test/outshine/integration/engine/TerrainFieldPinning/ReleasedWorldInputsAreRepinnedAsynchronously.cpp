#include "TerrainLattice.h"
#include "Check.h"
#include "DelayedTerrainSource.h"
#include "HeightSheets.h"
#include "TerrainInputPreparation.h"
#include "TerrainFieldPinning.h"
#include "RuntimeScene.h"
#include <SDL3/SDL.h>
#include <cstddef>
#include <utility>
#include <chrono>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;

template <typename Poll> bool UntilReady(Poll poll) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
  while (std::chrono::steady_clock::now() < deadline) {
    if (poll()) { return true; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  return false;
}

void CheckReleasedInputs(Generators::TerrainInputPreparation &inputs,
                         HeightSheets &published,
                         const Ground::GroundStream &ground,
                         const Ground::TilePool &pool,
                         DelayedTerrainSource &source,
                         Ground::HeightField::Block &retained) {
  const Data::TileId tile{.Zoom = 4, .X = 8, .Y = 8};
  Ground::HeightField::Block field;
  const auto posts = pool.Counters().Posts;
  CHECK(!ground.ResidentStitchedField(tile) && pool.Counters().Posts == posts,
        "a resident-only miss never posts work");
  CHECK(!PinTerrainField(&inputs, ground, tile, field) && !source.CalledOnCaller,
        "an unavailable field is pending without collecting on the caller");
  source.Released = true;
  CHECK(UntilReady([&] { return PinTerrainField(&inputs, ground, tile, field); }),
        "a published world without source fields repins through the existing producer");
  if (!field.Terrain) { return; }
  CHECK(field.Raster.Side == 4 && field.Terrain->Data()[0] == DelayedTerrainSource::HeightM &&
            !field.Sources.empty() && field.Certificate.ScopeCurrent(ground.TerrainScopeRevision()),
        "the returned pin owns the exact raster and current source certificate");
  Patchwork candidate;
  candidate.Sheets.push_back(
      {.Tile = tile,
       .Nodes = std::vector<float>(Render::TerrainLattice::kNodes, DelayedTerrainSource::HeightM),
       .Side = Render::TerrainLattice::kSide,
       .Postings = Render::TerrainLattice::kSide});
  CHECK(UntilReady([&] {
          const auto ready = inputs.PrepareFields(
              candidate, ground, {.FinestZoom = 4, .RequestsMost = 9, .AdditionalTiles = {}});
          return ready && *ready;
        }),
        "world preparation owns its complete source coverage");
  CHECK(inputs.ShareSourcedField(tile, retained), "a worker pins a consumed world input");
  inputs.Framed(TangentFrame::At({}));
  published.Framed(TangentFrame::At({}));
  std::string error;
  CHECK(inputs.Halos(candidate, 4) == candidate.Sheets.size() && published.Hands(candidate, error),
        "the sampled inputs become an independent published terrain surface");
  const LongitudeLatitude camera{.LongitudeDeg = 1.0, .LatitudeDeg = -1.0};
  const auto cameraHeight = published.HeightMAt(camera);
  const size_t before = inputs.HeapBytes();
  inputs.Reset();
  CHECK(inputs.HeapBytes() < before && !inputs.ShareSourcedField(tile, field),
        "completed world preparation releases its source ownership");
  CHECK(cameraHeight.has_value() && published.HeightMAt(camera) == cameraHeight &&
            published.PublishedSurface() != nullptr,
        "published camera and collision heights survive source release");
  CHECK(retained.Terrain && retained.Terrain->Data()[0] == DelayedTerrainSource::HeightM,
        "an independent worker pin survives world input release");
  CHECK(Ground::HeightField::SharesField(ground.ResidentStitchedField(tile), tile, field),
        "released world inputs can still use the bounded producer cache");
  source.Released = false;
  const Data::TileId moved{.Zoom = 4, .X = 2, .Y = 2};
  const auto residentPosts = pool.Counters().Posts;
  CHECK(!ground.ResidentStitchedField(moved) && pool.Counters().Posts == residentPosts,
        "moving beyond held coverage does not turn a resident query into IO");
  CHECK(!PinTerrainField(&inputs, ground, moved, field),
        "new runtime coverage starts as pending rather than an empty finished field");
  source.Released = true;
  CHECK(UntilReady([&] { return PinTerrainField(&inputs, ground, moved, field); }) &&
            field.At.X == 2 && field.At.Y == 2 && !source.CalledOnCaller,
        "new coverage becomes available asynchronously without cached native assets");
}
}

int main() {
  Ground::HeightField::Block retained;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Render::SceneRenderer renderer;
    const auto uploadBudget = renderer.OfflineUploads(std::chrono::seconds(5));
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) {
      Data::RawSourceCache store({.Directory = {}, .Using = Data::RawSourceCache::Use::Off});
      Data::SourceSet sources(store);
      NoNetwork transport;
      auto source = std::make_unique<DelayedTerrainSource>();
      auto *probe = source.get();
      CHECK(sources.Add(std::move(source)) == Data::SourceSet::Registration::Accepted,
            "delayed terrain fixture registered");
      Ground::TilePool::Config config;
      constexpr size_t decodedBudget = size_t{1024} * 1024u;
      constexpr int pollAttempts = 3000;
      config.DecodedBytes = decodedBudget;
      config.PollAttempts = pollAttempts;
      Ground::TilePool pool(config, sources, transport);
      const Ground::GroundStream ground(pool, {.Z = 4, .Grid = 4});
      Generators::TerrainInputPreparation inputs;
      HeightSheets published;
      published.Into(&renderer);
      CheckReleasedInputs(inputs, published, ground, pool, *probe, retained);
    }
  }
  CHECK(retained.Terrain && retained.Terrain->Data()[0] == DelayedTerrainSource::HeightM,
        "the worker pin owns its raster even after world and producer destruction");
  SDL_Quit();
  return Report();
}

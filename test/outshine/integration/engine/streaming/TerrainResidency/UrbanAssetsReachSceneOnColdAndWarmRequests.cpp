#include "TerrainPublicationFixture.h"
#include <cstdint>
#include "TerrainLattice.h"
#include "Check.h"
#include "UrbanAreaStreaming.h"
#include "streaming/TerrainResidency.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Geodesy.h"
#include "test/outshine/src/generators/osm/PreparedUrbanAreas/UrbanAssetFixture.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <system_error>
#include <vector>

namespace {
constexpr auto kAwaitSeconds = .05;
constexpr auto kMovementM = .25;
constexpr auto kTerrainHeightM = 42;
constexpr auto kEyeHeightM = 70;

using namespace outshine;
using namespace outshine::Test;

bool Pump(UrbanAreaStreaming &stream,
          const UrbanAreaStreaming::View &view,
          const TerrainResidency &terrain,
          std::span<const Ground::GeoBounds> children = {},
          uint64_t childrenRevision = 0) {
  const auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  do {
    const auto before = stream.Costs();
    const auto advanced = stream.Advance(view, terrain, 1, children, childrenRevision);
    CHECK(advanced, "the actual source/cache/terrain/scene consumer advances");
    if (!advanced) { return false; }
    const auto after = stream.Costs();
    CHECK(after.InFlight <= 2 && after.Captures - before.Captures <= 1 &&
              after.Publications - before.Publications <= 1,
          "all groups share bounded jobs, captures and scene publications");
    if (stream.Ready()) { return true; }
    (void)stream.AwaitProgress(kAwaitSeconds);
  } while (std::chrono::steady_clock::now() < until);
  return false;
}

void Probe(Render::SceneRenderer &renderer, const std::filesystem::path &directory) {
  using namespace UrbanAssetFixture;
  const auto at = Ground::TileFracToGeo({.X = kOwner.X + .5, .Y = kOwner.Y + .5}, kOwner.Zoom);
  const LongitudeLatitude focus{.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg};
  const auto frame = TangentFrame::At(focus);
  Vec3 eye;
  GeoToEcef(
      {.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = kEyeHeightM},
      eye);
  const UrbanAreaStreaming::View view{
      .Focus = focus, .EyeEcef = eye, .FocalPx = 1, .RadiusM = 240000, .Frame = frame};
  Patchwork patch;
  patch.Sheets.push_back(
      {.Tile = kOwner,
       .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes, kTerrainHeightM),
       .Side = Render::TerrainLattice::kSide,
       .Postings = Render::TerrainLattice::kSide});
  TerrainResidency terrain;
  terrain.Into(&renderer);
  std::string error;
  CHECK(PublishTerrain(terrain, patch, frame, error),
        "the consumer uses a published terrain surface");
  const auto database = (directory / "assets.sqlite").string();
  {
    Fixture fixture(directory, Availability::Ready);
    UrbanAreaStreaming stream(fixture.Compute, fixture.Urban, MvtSchema::OpenMapTiles, database);
    stream.Into(renderer);
    CHECK(Pump(stream, view, terrain), "cold source demand reaches resident native scene assets");
    const auto cold = stream.Costs();
    CHECK(cold.Sources == 1 && cold.Groups > 0 && cold.Resident == cold.Groups &&
              cold.Captures == cold.Groups && cold.Missing == 0,
          "cold groups generate once and all become visible scene resources");
    CHECK(Pump(stream, view, terrain) && stream.Costs().Captures == cold.Captures &&
              stream.Costs().Publications == cold.Publications,
          "an unchanged camera position does no generation or scene publication");
    auto moved = view;
    moved.EyeEcef = moved.EyeEcef + frame.EastEcef() * kMovementM;
    const auto geographic = GeoFromEcef(moved.EyeEcef);
    CHECK(geographic, "the moved eye remains a valid geographic position");
    if (geographic) {
      moved.Focus = {.LongitudeDeg = geographic->LongitudeDeg,
                     .LatitudeDeg = geographic->LatitudeDeg};
      CHECK(Pump(stream, moved, terrain) && stream.Costs().Captures == cold.Captures &&
                stream.Costs().Publications == cold.Publications,
            "movement within the same intrinsic groups does not create camera-specific assets");
    }
    auto unrelated = kOwner;
    unrelated.X += 5;
    const std::array distantChildren{Ground::TileBounds(unrelated)};
    CHECK(Pump(stream, view, terrain, distantChildren, 1) &&
              stream.Costs().Publications == cold.Publications &&
              stream.Costs().Captures == cold.Captures,
          "a distant child's publication retains unchanged local scene resources");
    const std::array children{Ground::TileBounds(kOwner)};
    CHECK(Pump(stream, view, terrain, children, 2) && stream.Costs().Resident == 0 &&
              stream.Costs().Captures == cold.Captures,
          "published children suppress parent cells without recooking cached assets");
    const auto covered = stream.Costs();
    CHECK(Pump(stream, view, terrain, children, 3) &&
              stream.Costs().Publications == covered.Publications,
          "unchanged covered groups acknowledge new revisions without republishing");
    CHECK(Pump(stream, view, terrain, {}, 4) && stream.Costs().Resident == cold.Resident &&
              stream.Costs().Captures == cold.Captures,
          "removing children restores the same cached product without terrain capture");
  }
  {
    Fixture fixture(directory, Availability::Unavailable);
    UrbanAreaStreaming stream(fixture.Compute, fixture.Urban, MvtSchema::OpenMapTiles, database);
    stream.Into(renderer);
    CHECK(Pump(stream, view, terrain), "a fresh consumer restores scene products from SSD");
    CHECK(stream.Costs().Resident == stream.Costs().Groups && stream.Costs().Captures == 0 &&
              fixture.Calls == 0 && fixture.Urban->Costs().Hits == 1,
          "warm source and geometry hits need neither network, source cook nor terrain capture");
  }
  terrain.Clear();
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-urban-world-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) { Probe(renderer, directory); }
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  SDL_Quit();
  return Report();
}

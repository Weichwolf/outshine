#include "TerrainPublicationFixture.h"
#include "test/outshine/src/generators/osm/PreparedUrbanAreas/UrbanAssetFixture.h"
#include "UrbanAreaStreaming.h"
#include "TerrainLattice.h"
#include "streaming/TerrainResidency.h"
#include "RuntimeScene.h"
#include "SceneRenderer.h"
#include "Geodesy.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <sqlite3.h>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace UrbanAssetFixture;
constexpr auto kDeadline = std::chrono::seconds(10);

void VerifyHeld(UrbanAreaStreaming &stream,
                const UrbanAreaStreaming::View &view,
                const TerrainResidency &terrain) {
  auto deadline = std::chrono::steady_clock::now() + kDeadline;
  bool reached = false;
  while (std::chrono::steady_clock::now() < deadline) {
    const auto value = stream.Advance(view, terrain, 1, {}, 0);
    CHECK(value, "a held prism commit does not fail the actual world-streaming consumer");
    if (!value) { return; }
    const auto costs = stream.Costs();
    CHECK(costs.InFlight <= 2 && costs.Admitted <= 2 && costs.Captures <= 2 &&
              costs.Resident == 0 && costs.Publications == 0,
          "all deferred and running packages share the original two admissions without early scene "
          "publication");
    if (!reached && costs.Captures == 2 && costs.InFlight == 0 && costs.Admitted == 2) {
      reached = true;
      deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(80);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto costs = stream.Costs();
  CHECK(
      reached && costs.Groups > 2 && costs.Captures == 2 && costs.Admitted == 2 && !stream.Ready(),
      "two idle contended products retain admission while additional visible groups stay deferred");
}

void VerifyReleased(UrbanAreaStreaming &stream,
                    const UrbanAreaStreaming::View &view,
                    const TerrainResidency &terrain) {
  const auto deadline = std::chrono::steady_clock::now() + kDeadline;
  while (!stream.Ready() && std::chrono::steady_clock::now() < deadline) {
    const auto value = stream.Advance(view, terrain, 1, {}, 0);
    CHECK(value && stream.Costs().InFlight <= 2 && stream.Costs().Admitted <= 2,
          "the same bounded consumer resumes all native products after admission returns");
    if (!value) { return; }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  const auto costs = stream.Costs();
  CHECK(stream.Ready() && costs.Resident == costs.Groups && costs.Captures == costs.Groups &&
            costs.Admitted == 0 && costs.InFlight == 0,
        "each demanded group reaches the actual native scene once and releases its admission");
}

void Probe(Render::SceneRenderer &renderer, const std::filesystem::path &directory) {
  const auto at = Ground::TileFracToGeo({.X = kOwner.X + .5, .Y = kOwner.Y + .5}, kOwner.Zoom);
  const LongitudeLatitude focus{.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg};
  const auto frame = TangentFrame::At(focus);
  Vec3 eye;
  GeoToEcef({.LongitudeDeg = at.LongitudeDeg, .LatitudeDeg = at.LatitudeDeg, .HeightM = 70}, eye);
  const UrbanAreaStreaming::View view{
      .Focus = focus, .EyeEcef = eye, .FocalPx = 1, .RadiusM = 240000, .Frame = frame};
  Patchwork patch;
  patch.Sheets.push_back({.Tile = kOwner,
                          .Nodes = std::vector<float>(Render::TerrainLattice::kPageNodes, 42),
                          .Side = Render::TerrainLattice::kSide,
                          .Postings = Render::TerrainLattice::kSide});
  TerrainResidency terrain;
  terrain.Into(&renderer);
  std::string error;
  CHECK(PublishTerrain(terrain, patch, frame, error),
        "the real renderer publishes source height pages");
  Fixture fixture(directory, Availability::Ready);
  const auto ready =
      UrbanAssetFixture::Await(fixture, {kOwner, MvtSchema::OpenMapTiles, {.PatchSide = 32}});
  CHECK(ready && ready->Product,
        "native OSM/urban source products commit before prism contention starts");
  if (!ready || !ready->Product) { return; }
  const auto sources = fixture.Calls.load();
  sqlite3 *raw = nullptr;
  const int opened = sqlite3_open((directory / "assets.sqlite").c_str(), &raw);
  const std::unique_ptr<sqlite3, decltype(&sqlite3_close)> writer(raw, sqlite3_close);
  CHECK(opened == SQLITE_OK, "a separate SQLite connection opens after source preparation");
  if (opened != SQLITE_OK) { return; }
  CHECK(sqlite3_exec(writer.get(), "BEGIN IMMEDIATE", nullptr, nullptr, nullptr) == SQLITE_OK,
        "an independent real writer blocks only the remaining native prism publication");
  {
    UrbanAreaStreaming stream(fixture.Compute,
                              fixture.Urban,
                              *fixture.Pool,
                              MvtSchema::OpenMapTiles,
                              (directory / "assets.sqlite").string());
    stream.Into(renderer);
    VerifyHeld(stream, view, terrain);
    CHECK(fixture.Calls == sources,
          "pending prism commits neither fetch nor cook more source inputs");
    bool progressed = false;
    const auto other = fixture.Compute.Post([&progressed] { progressed = true; });
    CHECK(other != Tasks::kNoTask, "unrelated work enters the same compute pool");
    if (other != Tasks::kNoTask) { fixture.Compute.Wait(other); }
    CHECK(progressed, "the held logical admissions leave the actual worker available");
    CHECK(sqlite3_exec(writer.get(), "ROLLBACK", nullptr, nullptr, nullptr) == SQLITE_OK,
          "the external transaction releases native prism publication");
    VerifyReleased(stream, view, terrain);
  }
  terrain.Clear();
}
}

int main() {
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes for the actual native renderer");
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("outshine-urban-prism-contention-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  {
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "the native world opens");
    if (scene) { Probe(renderer, directory); }
  }
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  SDL_Quit();
  return Report();
}

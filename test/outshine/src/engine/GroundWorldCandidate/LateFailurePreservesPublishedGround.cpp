#include "GroundWorldCandidate.h"
#include "Check.h"
#include "test/harness/render/NativeUploadFaultDevice.h"
#include <SDL3/SDL.h>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace {
constexpr size_t kReplacementWayCount = 19;

void RefusePublicationDuringCapture(outshine::Render::SceneRenderer &renderer,
                                    outshine::Surrounds &world,
                                    outshine::Generators::Osm::BuildingField &footprints,
                                    std::unique_ptr<outshine::Core::RuntimeScene> &scene,
                                    outshine::GroundRevision nextRevision) {
  using namespace outshine;
  using namespace outshine::Test;
  const auto *oldScene = scene.get();
  const auto oldRegion = world.Region;
  {
    GroundWorldCandidate held(renderer, world, footprints);
    const auto heldPrepared = held.Prepare(*scene, nullptr);
    CHECK(heldPrepared.has_value(),
          heldPrepared ? "capture candidate prepares against the published world"
                       : heldPrepared.error().c_str());
    if (heldPrepared) {
      held.Products().PositionsM = {9, 8, 7};
      held.Products().Indices = {0, 0, 0};
      held.Products().StreetGraphWayCount = kReplacementWayCount;
      CHECK(world.GroundPublished.BeginCapture(), "published ground enters capture");
      const auto heldResult = held.Publish(world, footprints, scene, nextRevision);
      CHECK(!heldResult && scene.get() == oldScene &&
                world.GroundPositionsM == std::vector<float>({1, 2, 3}) &&
                world.StreetGraphWayCount == 7 && world.Relaid == 0 && world.Region == oldRegion,
            "capture refuses a late candidate before changing live or CPU world products");
      world.GroundPublished.EndCapture();
    }
  }
}

void PublishRetry(outshine::Render::SceneRenderer &renderer,
                  outshine::Surrounds &world,
                  outshine::Generators::Osm::BuildingField &footprints,
                  std::unique_ptr<outshine::Core::RuntimeScene> &scene,
                  const outshine::Geometry &geometry) {
  using namespace outshine;
  using namespace outshine::Test;
  const auto *oldScene = scene.get();
  const auto oldRegion = world.Region;
  const GroundRevision nextRevision{.Region = 18};
  std::string error;
  GroundWorldCandidate retry(renderer, world, footprints);
  const auto prepared = retry.Prepare(*scene, nullptr);
  CHECK(prepared.has_value(),
        prepared ? "an immediate retry can acquire the abandoned candidate slot"
                 : prepared.error().c_str());
  if (prepared) {
    retry.Products().PositionsM = {9, 8, 7};
    retry.Products().Indices = {0, 0, 0};
    retry.Products().StreetGraphWayCount = kReplacementWayCount;
    retry.Products().RimsMissing = 0;
    CHECK(renderer.SetTerrainTiles({}, {}, error) &&
              retry.SetGroundGeometry(geometry.clone(), 0, error),
          "retry finishes GPU preparation");
    CHECK(retry.Publish(world, footprints, scene, nextRevision).has_value(),
          "complete retry publishes");
    CHECK(
        scene.get() != oldScene && renderer.GroundLatticeTriangles() == 0 &&
            scene->DrivenParts() == 0 && world.GroundPositionsM == std::vector<float>({9, 8, 7}) &&
            world.GroundIndex == std::vector<uint32_t>({0, 0, 0}) &&
            world.StreetGraphWayCount == kReplacementWayCount && world.RimsMissing == 0 &&
            world.Relaid == 1 && !world.GroundPublished.NeedsRebuild(nextRevision, false, false) &&
            world.Region != oldRegion,
        "GPU terrain, semantic source, CPU products and revision publish together once");
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    outshine::Test::Gpu::UploadFailure failure{.Message =
                                                   "injected late ground publication failure"};
    Render::SceneRenderer renderer(
        {.Context = &failure, .Create = outshine::Test::Gpu::NativeUploadFaultDevice::Create});
    const auto uploadBudget = renderer.OfflineUploads(std::chrono::seconds(5));
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::RuntimeScene> scene;
    std::string error;
    CHECK(Core::RuntimeScene::Open(renderer, declaration, nullptr, scene, error),
          "initial world opens");
    if (scene) {
      Surrounds world;
      ::outshine::Generators::Osm::BuildingField footprints;
      world.BindSceneResources(renderer);
      world.GroundPositionsM = {1, 2, 3};
      world.GroundIndex = {0};
      world.StreetGraphWayCount = 7;
      world.RimsMissing = 2;
      const auto oldRegion = std::make_shared<const ::outshine::Generators::Osm::PublishedRegion>(
          ::outshine::Generators::Osm::RegionSources{},
          ::outshine::Generators::Osm::BuildingField{});
      world.Region = oldRegion;
      const GroundRevision oldRevision{.Region = 17};
      const GroundRevision nextRevision{.Region = 18};
      CHECK(world.GroundPublished.Publish(oldRevision), "original ground revision publishes");
      const Core::RuntimeScene *const oldScene = scene.get();
      std::vector<float> nodes(Render::GroundLattice::kPageNodes, 3.0f);
      const auto page = renderer.PlaceHeightPage(nodes);
      CHECK(page.has_value(), "original height page uploads");
      if (!page) { return Report(); }
      std::vector<float> fractions(Render::GroundLattice::kSide);
      for (size_t at = 0; at < fractions.size(); ++at) {
        fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
      }
      Render::TerrainTile tile;
      tile.Corners = {{-1, -1, 1, -1, -1, 1, 1, 1}};
      tile.Page = *page;
      tile.LowM = tile.HighM = 3.0f;
      CHECK(renderer.SetGroundGrid(fractions, error) &&
                renderer.SetTerrainTiles({&tile, 1}, {}, error),
            "original world has a complete terrain lattice");
      Geometry geometry;
      const auto part = geometry.addPart("triangle", MaterialInstance{});
      CHECK(part &&
                geometry.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
                geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
            "replacement native geometry is complete");
      for (const bool rejectGeometry : {false, true}) {
        {
          GroundWorldCandidate build(renderer, world, footprints);
          const auto prepared = build.Prepare(*scene, nullptr);
          CHECK(prepared.has_value(),
                prepared ? "terrain candidate prepares from the active world"
                         : prepared.error().c_str());
          if (prepared) {
            build.Products().PositionsM = {9, 8, 7};
            build.Products().Indices = {0, 0, 0};
            build.Products().StreetGraphWayCount = kReplacementWayCount;
            build.Products().RimsMissing = 0;
            CHECK(renderer.SetTerrainTiles({}, {}, error), "candidate changes terrain first");
            failure.Armed = true;
            const bool accepted =
                rejectGeometry ? build.SetGroundGeometry(geometry.clone(), 0, error)
                               : renderer.SetGroundClasses(std::array<uint32_t, 4>{1, 2, 3, 4},
                                                           std::array<float, 4>{1, 0.5f, 0.25f, 0},
                                                           error);
            CHECK(
                !accepted && !failure.Armed,
                (std::string("a later GPU upload fails after terrain staging: ") + error).c_str());
            failure.Armed = false;
          }
        }
        CHECK(scene.get() == oldScene &&
                  renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
              "abandonment preserves the published owner and original GPU terrain");
        CHECK(world.GroundPositionsM == std::vector<float>({1, 2, 3}) &&
                  world.GroundIndex == std::vector<uint32_t>({0}) &&
                  world.StreetGraphWayCount == 7 && world.RimsMissing == 2 && world.Relaid == 0 &&
                  world.GroundPublished.NeedsRebuild(nextRevision, false, false) &&
                  world.Region == oldRegion,
              "late failure preserves CPU terrain, semantic source and publication revision");
      }
      RefusePublicationDuringCapture(renderer, world, footprints, scene, nextRevision);
      PublishRetry(renderer, world, footprints, scene, geometry);
    }
  }
  SDL_Quit();
  return Report();
}

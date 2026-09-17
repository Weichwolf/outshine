#include "GroundWorldCandidate.h"
#include "Check.h"
#include <SDL3/SDL.h>
#include <array>
#include <cassert>
#include <dlfcn.h>

namespace {
bool rejectSubmit = false;

bool RejectCommands(SDL_GPUCommandBuffer *commands) {
  if (!rejectSubmit) { return false; }
  rejectSubmit = false;
  const bool cancelled = SDL_CancelGPUCommandBuffer(commands);
  assert(cancelled);
  SDL_SetError("injected late ground publication failure");
  return true;
}
}

extern "C" bool SDLCALL SDL_SubmitGPUCommandBuffer(SDL_GPUCommandBuffer *commands) {
  if (RejectCommands(commands)) { return false; }
  static const auto original = reinterpret_cast<decltype(&SDL_SubmitGPUCommandBuffer)>(
      dlsym(RTLD_NEXT, "SDL_SubmitGPUCommandBuffer"));
  assert(original != nullptr);
  return original(commands);
}

extern "C" SDL_GPUFence *SDLCALL
SDL_SubmitGPUCommandBufferAndAcquireFence(SDL_GPUCommandBuffer *commands) {
  if (RejectCommands(commands)) { return nullptr; }
  static const auto original =
      reinterpret_cast<decltype(&SDL_SubmitGPUCommandBufferAndAcquireFence)>(
          dlsym(RTLD_NEXT, "SDL_SubmitGPUCommandBufferAndAcquireFence"));
  assert(original != nullptr);
  return original(commands);
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  CHECK(SDL_Init(SDL_INIT_VIDEO), "video initializes");
  {
    Render::SceneRenderer renderer;
    Core::Declaration declaration;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "initial world opens");
    if (scene) {
      Surrounds world;
      Ground::BuildingField footprints;
      world.BindLiveResources(*scene);
      world.GroundPositionsM = {1, 2, 3};
      world.GroundIndex = {0};
      world.NetworkOfWays = 7;
      world.RimsMissing = 2;
      const GroundRevision oldRevision{.Region = 17};
      const GroundRevision nextRevision{.Region = 18};
      world.GroundPublished.Publish(oldRevision);
      Core::Live *const oldScene = scene.get();
      std::vector<float> nodes(Render::GroundLattice::kPageNodes, 3.0f);
      const auto page = scene->PlaceHeightPage(nodes);
      CHECK(page.has_value(), "original height page uploads");
      if (!page) { return Report(); }
      std::vector<float> fractions(Render::GroundLattice::kSide);
      for (size_t at = 0; at < fractions.size(); ++at) {
        fractions[at] = static_cast<float>(at) / static_cast<float>(fractions.size() - 1u);
      }
      Core::GroundTile tile;
      tile.Corners = {{-1, -1, 1, -1, -1, 1, 1, 1}};
      tile.Page = *page;
      tile.LowM = tile.HighM = 3.0f;
      CHECK(scene->SetGroundGrid(fractions, error) &&
                scene->SetGroundLattice({&tile, 1}, {}, error),
            "original world has a complete terrain lattice");
      Geometry geometry;
      const auto part = geometry.addPart("triangle", MaterialInstance{});
      CHECK(part &&
                geometry.setPositions(*part, std::array<float, 9>{-1, -1, 0, 1, -1, 0, 0, 1, 0}) &&
                geometry.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
            "replacement native geometry is complete");
      for (bool rejectGeometry : {false, true}) {
        {
          GroundWorldCandidate build(renderer, world, footprints);
          const auto prepared = build.Prepare(*scene, nullptr);
          CHECK(prepared.has_value(), "terrain candidate prepares from the active world");
          if (prepared) {
            build.Products().PositionsM = {9, 8, 7};
            build.Products().Indices = {0, 0, 0};
            build.Products().NetworkOfWays = 19;
            build.Products().RimsMissing = 0;
            CHECK(build.Scene().SetGroundLattice({}, {}, error), "candidate changes terrain first");
            rejectSubmit = true;
            const bool accepted =
                rejectGeometry
                    ? build.Scene().SetGeometry(geometry.clone(), 0, error)
                    : build.Scene().GroundClasses(std::array<uint32_t, 4>{1, 2, 3, 4},
                                                  std::array<float, 4>{1, 0.5f, 0.25f, 0},
                                                  error);
            CHECK(
                !accepted && !rejectSubmit,
                (std::string("a later GPU upload fails after terrain staging: ") + error).c_str());
            rejectSubmit = false;
          }
        }
        CHECK(scene.get() == oldScene &&
                  renderer.GroundLatticeTriangles() == Render::GroundLattice::kIndices / 3u,
              "abandonment preserves the published owner and original GPU terrain");
        CHECK(world.GroundPositionsM == std::vector<float>({1, 2, 3}) &&
                  world.GroundIndex == std::vector<uint32_t>({0}) && world.NetworkOfWays == 7 &&
                  world.RimsMissing == 2 && world.Relaid == 0 &&
                  world.GroundPublished.NeedsRebuild(nextRevision, false, false),
              "late failure preserves CPU terrain, network metadata and publication revision");
      }
      GroundWorldCandidate retry(renderer, world, footprints);
      const auto prepared = retry.Prepare(*scene, nullptr);
      CHECK(prepared.has_value(), "an immediate retry can acquire the abandoned candidate slot");
      if (prepared) {
        retry.Products().PositionsM = {9, 8, 7};
        retry.Products().Indices = {0, 0, 0};
        retry.Products().NetworkOfWays = 19;
        retry.Products().RimsMissing = 0;
        CHECK(retry.Scene().SetGroundLattice({}, {}, error) &&
                  retry.Scene().SetGeometry(geometry.clone(), 0, error),
              "retry finishes GPU preparation");
        CHECK(retry.Publish(world, footprints, scene, nextRevision).has_value(),
              "complete retry publishes");
        CHECK(scene.get() != oldScene && renderer.GroundLatticeTriangles() == 0 &&
                  world.GroundPositionsM == std::vector<float>({9, 8, 7}) &&
                  world.GroundIndex == std::vector<uint32_t>({0, 0, 0}) &&
                  world.NetworkOfWays == 19 && world.RimsMissing == 0 && world.Relaid == 1 &&
                  !world.GroundPublished.NeedsRebuild(nextRevision, false, false),
              "GPU terrain, CPU products and revision publish together exactly once");
      }
    }
  }
  SDL_Quit();
  return Report();
}

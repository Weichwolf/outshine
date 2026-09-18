#include "WorldCandidate.h"
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
  SDL_SetError("injected piece upload failure");
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
    Geometry base;
    const auto surface = base.addSurface("wall", Material{});
    CHECK(surface.has_value(), "fixture material exists");
    if (!surface) { return Report(); }
    const auto part = base.addPart("base", *surface);
    CHECK(part && base.setPositions(*part, std::array<float, 9>{0, 0, 0, 1, 0, 0, 0, 1, 0}) &&
              base.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
          "fixture has a surface");
    Core::Declaration declaration;
    declaration.InitialGeometry = &base;
    declaration.SurfaceWidthPx = declaration.SurfaceHeightPx = 32;
    declaration.Outputs = {"surface"};
    Render::SceneRenderer renderer;
    std::unique_ptr<Core::Live> scene;
    std::string error;
    CHECK(Core::Live::Open(renderer, declaration, nullptr, scene, error), "world opens");
    if (scene) {
      const std::array<StoredVertex, 3> vertices{
          StoredVertex::Of({{0, 0, 0}}, {{0, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{1, 0, 0}}, {{1, 0}}, {{0, 0, 1}}),
          StoredVertex::Of({{0, 1, 0}}, {{0, 1}}, {{0, 0, 1}})};
      const std::array<uint32_t, 3> indices{0, 1, 2};
      const Render::PieceMesh mesh{.Verts = vertices, .Indices = indices};
      const auto first = renderer.PlacePiece(mesh);
      const auto second = renderer.PlacePiece(mesh);
      CHECK(first && second, "two native identities exist");
      if (!first || !second) { return Report(); }
      renderer.ReleasePiece(*first);
      const auto bytes = renderer.PieceSourceBytes();
      {
        Core::WorldCandidate rejected(renderer);
        const auto prepared = rejected.Prepare(*scene, nullptr);
        CHECK(prepared.has_value(), "candidate copies live and free slots");
        if (!prepared) { return Report(); }
        CHECK(renderer.SetPieceInstances(*second, {}, error),
              "native slot one resolves after GPU recreation compacts away slot zero");
        const auto temporary = renderer.PlacePiece(mesh);
        CHECK(temporary && temporary->Slot == first->Slot &&
                  temporary->Generation == first->Generation + 1,
              "candidate reuses its copied free slot");
      }
      CHECK(renderer.PiecesStanding() == 1 && renderer.PieceSourceBytes() == bytes &&
                renderer.SetPieceInstances(*second, {}, error),
            "candidate rejection preserves the original identity and payload");
      Core::WorldCandidate candidate(renderer);
      const auto prepared = candidate.Prepare(*scene, nullptr);
      CHECK(prepared.has_value(), "retry prepares");
      if (!prepared) { return Report(); }
      rejectSubmit = true;
      const auto failed = renderer.PlacePiece(mesh);
      CHECK(!failed && !rejectSubmit && renderer.PieceSlots() == 2 &&
                renderer.PieceSourceBytes() == bytes,
            "late GPU upload failure consumes neither slot nor payload");
      const auto replacement = renderer.PlacePiece(mesh);
      CHECK(replacement && replacement->Slot == first->Slot &&
                replacement->Generation == first->Generation + 1,
            "retry keeps the free slot generation unchanged after failure");
      if (!replacement) { return Report(); }
      CHECK(candidate.Publish(scene).has_value(), "complete resource world publishes");
      CHECK(renderer.SetPieceInstances(*second, {}, error) &&
                renderer.SetPieceInstances(*replacement, {}, error) &&
                !renderer.SetPieceInstances(*first, {}, error),
            "publication preserves valid native handles and rejects the released generation");
      const Render::PieceHandle outside{.Slot = Render::kNoResourceSlot - 1, .Generation = 1};
      CHECK(!renderer.SetPieceInstances(outside, {}, error), "out-of-range handle is rejected");
      renderer.ReleasePiece(outside);
      renderer.ReleasePiece(*first);
      CHECK(renderer.PiecesStanding() == 2, "invalid releases preserve both current pieces");
      renderer.ReleasePiece(*second);
      renderer.ReleasePiece(*replacement);
      CHECK(renderer.PieceSourceBytes() == 0 && renderer.PiecesStanding() == 0,
            "published handles release the reconstructed resources");
    }
  }
  SDL_Quit();
  return Report();
}
